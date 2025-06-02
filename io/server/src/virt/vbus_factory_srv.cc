/*
 * Copyright (C) 2025-2026 Kernkonzept GmbH.
 * Author(s): Jean Wolter <jean.wolter@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/util/meta>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>

#include <l4/sys/factory>
#include <l4/sys/task>
#include <l4/sys/debugger.h>

#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_varg>
#include <l4/cxx/string>

#include <l4/re/util/debug>
#include <l4/re/error_helper>

#include "vbus_factory_srv.h"

IO_factory::IO_factory() : _del_cap_irq{this}
{
  char const *svr = "svr";
  L4::Cap<L4::Rcv_endpoint> cap =
    L4Re::Env::env()->get_cap<L4::Rcv_endpoint>(svr);

  if (!cap)
    {
      d_printf(DBG_ERR,
               "Missing server endpoint %s, dynamic factory interface not available\n",
               svr);
      return;
    }

  if (!registry->register_obj(this, cap))
    {
      d_printf(DBG_ERR, "Could not register factory interface on '%s': %s,"
               "dynamic factory interface not available\n",
               svr, l4sys_errtostr(cap.cap()));
      return;
    }

  l4_debugger_set_object_name(cap.cap(), "IO_Factory");

  _active = true;
  auto c = L4Re::chkcap(registry->register_irq_obj(&_del_cap_irq),
                        "Failed to acquire deletion IRQ object for dynamic client management.");
  L4Re::chksys(L4Re::Env::env()->main_thread()->register_del_irq(c),
               "Failed to register deletion IRQ with kernel for dynamic client management.");
}

void IO_factory::handle_irq()
{
  // Check for orphaned Vbus endpoints
  for (auto *vbus : _busses)
    {
      if (vbus->obj_cap() && !vbus->obj_cap().validate().label())
        {
          auto c = vbus->obj_cap();
          // reset dma domains here
          vbus->reset_dma_domains();
          d_printf(DBG_INFO,
                   "Client on vbus %p %s has gone. Releasing cap %lx\n",
                   vbus, vbus->name(), c.cap());
          registry->unregister_obj(vbus);
        }
    }
}

Vi::System_bus *IO_factory::lookup_vbus(char const *opt_str, unsigned len)
{
  for (auto *vbus : _busses)
    {
      auto *name = vbus->name();
      if (!strncmp(name, opt_str, len))
        return vbus;
    }
  return nullptr;
}

void IO_factory::add_vbus(Vi::System_bus *vbus)
{
  _busses.push_back(vbus);
}

long IO_factory::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                           l4_umword_t type, L4::Ipc::Varg_list_ref va)
{
  if (type != 0)
    return -L4_EINVAL;

  L4::Ipc::Varg opt = va.pop_front();
  if (!opt.is_of<char const *>())
    return -L4_EINVAL;

  char const *pref = "vbus=";
  const size_t pref_len = strlen(pref);
  unsigned len = opt.length();
  char const *opt_str = opt.data();
  if (len <= pref_len || strncmp(pref, opt_str, pref_len))
    return -L4_EINVAL;

  auto *vbus = lookup_vbus(opt_str + pref_len, len - pref_len);

  if (!vbus)
    return -L4_ENODEV;

  if (vbus->obj_cap())
    return -L4_EEXIST;

  // register Vbus endpoint
  L4Re::chkcap(registry->register_obj(vbus), "Register vbus endpoint");

  // decrement ref counter to get a notification when the last
  // external reference vanishes
  vbus->obj_cap()->dec_refcnt(1);

  d_printf(DBG_INFO, "Created Vbus endpoint 0x%lx for '%s'\n",
           vbus->obj_cap().cap() >> L4_CAP_SHIFT, vbus->name());

  res = vbus->obj_cap();
  return L4_EOK;
}

static IO_factory factory;
IO_factory *IO_factory::get()
{
  return &factory;
}
