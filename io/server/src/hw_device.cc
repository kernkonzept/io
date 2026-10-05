/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "hw_device.h"
#include "cfg.h"
#include "debug.h"

bool
Hw::Device::setup()
{
  // FIXME: check and initialize things we depend on
  for (Feature_list::const_iterator d = _features.begin();
       d != _features.end(); ++d)
    (*d)->setup(this);

  return true;
}

int
Hw::Device::pm_suspend()
{
  if (pm_power_state() != Pm_online)
    return 0;

  for (Feature_list::const_iterator d = _features.begin();
       d != _features.end(); ++d)
    (*d)->pm_save_state(this);

  pm_set_state(Pm_suspended);

  return 0;
}

int
Hw::Device::pm_resume()
{
  if (pm_power_state() != Pm_suspended)
    return 0;

  for (Feature_list::const_iterator d = _features.begin();
       d != _features.end(); ++d)
    (*d)->pm_restore_state(this);

  pm_set_state(Pm_online);

  return 0;
}

void
Hw::Device::setup_children()
{
  for (Feature_list::const_iterator d = _features.begin();
       d != _features.end(); ++d)
    (*d)->setup_children(this);
}

int
Hw::Device::pm_init()
{
#if 0
  printf("Hw::Device::plug(this=%p, name='%s', hid='%s')\n",
         this, name(), hid());
#endif
  allocate_pending_resources();
  if (!setup())
    {
      pm_set_state(Pm_failed);
      return -ENODEV;
    }

  _sta = Active;

  setup_children();

  pm_set_state(Pm_online);
  return 0;
}

void
Hw::Device::init()
{
  if (0)
    printf("Hw::Device::plug(this=%p, name='%s', hid='%s')\n",
           this, name(), hid());

  if (_flags & DF_dma_supported)
    if (!dma_domain() && parent())
      parent()->dma_domain_for(this);

  int r = pm_init();
  if (r < 0)
    {
      d_printf(DBG_ERR, "error: failed to setup device: %s\n", name());
      return;
    }

  for (iterator c = begin(0); c != end(); ++c)
    (*c)->init();
}

void
Hw::Device::plugin()
{
  init();
}

void
Hw::Device_factory::dump()
{
  for (Name_map::const_iterator i = nm().begin(); i != nm().end(); ++i)
    printf("HW: '%s'\n", (*i).first.c_str());
}

Hw::Device *
Hw::Device_factory::create(cxx::String const &name)
{
  Name_map::const_iterator i = nm().find(std::string(name.start(), name.end()));
  if (i != nm().end())
    return i->second->create();

  return 0;
}

bool
Hw::Device::resource_allocated(Resource const *r) const
{
  return r->parent();
}


Hw::Device *
Hw::Device::get_child_dev_adr(l4_uint32_t adr, bool create)
{
  for (Hw::Device *c = children(); c; c = c->next())
    if (c->adr() == adr)
      return c;

  if (!create)
    return 0;

  Hw::Device *c = new Hw::Device(adr);
  add_child(c);
  return c;
}

Hw::Device *
Hw::Device::get_child_dev_uid(l4_umword_t uid, l4_uint32_t adr, bool create)
{
  for (Hw::Device *c = children(); c; c = c->next())
    if (c->uid() == uid)
      return c;

  if (!create)
    return 0;

  Hw::Device *c = new Hw::Device(uid, adr);
  add_child(c);
  return c;
}

bool
Hw::Device::match_cid(cxx::String const &cid) const
{
  if (Generic_device::match_cid(cid))
    return true;

  for (Cid_list::const_iterator i = _cid.begin(); i != _cid.end(); ++i)
    if (cid == (*i).c_str())
      return true;

  for (Feature_list::const_iterator i = _features.begin();
       i != _features.end(); ++i)
    if ((*i)->match_cid(cid))
      return true;

  return false;
}

void
Hw::Device::dump(int indent) const
{
  printf("%*.s%s: %s%s\n", indent, " ", name(),
         hid() && hid()[0] ? "hid=" : "", hid() ? hid() : "");

  if (!_cid.empty())
    {
      bool first = true;
      printf("%*.s  compatible= { ", indent, " ");
      for (auto const &i : _cid)
        {
          printf("%s\"%s\"", first ? "" : ", ", i.c_str());
          first = false;
        }
      puts(" }");
    }

  if (Io_config::cfg->verbose() > 2)
    {
      for (Feature_list::const_iterator i = _features.begin();
           i != _features.end(); ++i)
        (*i)->dump(indent + 2);

      if (!_clients.empty())
        {
          printf("%*.s  Clients: ===== start ====\n", indent, " ");
          for (auto i = _clients.begin(); i != _clients.end(); ++i)
            (*i)->dump(indent + 4);
          printf("%*.s  Clients: ===== end ====\n", indent, " ");
        }
    }
}

void
Hw::Device::add_feature(Hw::Dev_feature *f)
{
  _features.push_back(f);
  Hw::Feature_manager_base::match(this, f);
  if (!_clients.empty())
    f->enable_notifications(this);
}

void
Hw::Device::check_conflicts() const
{
  for (auto i = _clients.begin(); i != _clients.end(); ++i)
    for (auto j = i; ++j != _clients.end();)
      {
        if (!(*j)->check_conflict(*i))
          continue;

        d_printf(DBG_WARN, "warning: conflicting virtual clients:\n"
                           "  %s\n"
                           "  %s\n",
                 (*j)->get_full_name().c_str(),
                 (*i)->get_full_name().c_str());

      }
}

void
Hw::Device::add_client(Hw::Device_client *client)
{
  bool first = _clients.empty();
  _clients.push_front(client);
  if (first)
    for (auto i = _features.begin(); i != _features.end(); ++i)
      (*i)->enable_notifications(this);
}

void
Hw::Device::notify(unsigned type, unsigned event, unsigned value)
{
  for (auto i = _clients.begin(); i != _clients.end(); ++i)
    (*i)->notify(type, event, value);

  if (_clients.empty())
    for (auto i = _features.begin(); i != _features.end(); ++i)
      (*i)->disable_notifications(this);

}

namespace {
  static Hw::Device_factory_t<Hw::Device> __hw_pf_factory("Device");
}
