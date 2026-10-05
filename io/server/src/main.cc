/*
 * (c) 2010-2020 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include <l4/sys/capability>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/namespace>
#include <l4/sys/icu>
#include <l4/sys/iommu>
#include <l4/re/error_helper>
#include <l4/re/dataspace>
#include <l4/util/kip.h>

#include "main.h"
#include "hw/hw_root_bus.h"
#include "hw/hw_device.h"
#include "server.h"
#include "res.h"
#include "platform_control.h"
#include "__acpi.h"
#include "virt/vbus.h"
#include "virt/vbus_factory.h"
#include "virt/vbus_factory_srv.h"
#include "phys_space.h"
#include "cfg.h"

#include <cstdio>
#include <typeinfo>
#include <algorithm>
#include <string>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

namespace {

static Hw::Root_bus *
hw_system_bus()
{
  static Hw::Root_bus _sb("System Bus");
  return &_sb;
}

static Platform_control *
platform_control()
{
  static Platform_control pfc(hw_system_bus());
  return &pfc;
}


struct Virtual_sbus : Vi::System_bus
{
  Virtual_sbus() : Vi::System_bus(platform_control()) {}
};

static Vi::Dev_factory_t<Virtual_sbus> __sb_root_factory("System_bus");

}

int luaopen_Io(lua_State *);

static const luaL_Reg libs[] =
{
  {"_G", luaopen_base },
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_STRLIBNAME, luaopen_string },
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_TABLIBNAME, luaopen_table},
  { NULL, NULL }
};

class Io_config_x : public Io_config
{
public:
  Io_config_x()
  : _do_transparent_msi(false), _verbose_lvl(1) {}

  bool transparent_msi(Hw::Device *) const override
  { return _do_transparent_msi; }

  bool legacy_ide_resources(Hw::Device *) const override
  { return true; }

  bool expansion_rom(Hw::Device *) const override
  { return false; }

  void set_transparent_msi(bool v) { _do_transparent_msi = v; }

  int verbose() const override { return _verbose_lvl; }
  void inc_verbosity() { ++_verbose_lvl; }

private:
  bool _do_transparent_msi;
  int _verbose_lvl;
};

static Io_config_x _my_cfg __attribute__((init_priority(30000)));
Io_config *Io_config::cfg = &_my_cfg;


Hw::Device *
system_bus()
{
  return hw_system_bus();
}

Hw_icu *
system_icu()
{
  static Hw_icu _icu;
  return &_icu;
}

static void check_conflicts(Hw::Device *d)
{
  for (auto i = Hw::Device::iterator(0, d, 100); i != d->end(); ++i)
    (*i)->check_conflicts();
}

void dump_devs(Device *d);
int add_vbus(Vi::Device *dev);

void dump_devs(Device *d) { dump(d); }


Hw_icu::Hw_icu()
{
  icu = L4Re::Env::env()->get_cap<L4::Icu>("icu");
  if (icu.is_valid())
    icu->info(&info);
}

int add_vbus(Vi::Device *dev)
{
  Vi::System_bus *b = dynamic_cast<Vi::System_bus*>(dev);
  if (!b)
    {
      d_printf(DBG_ERR, "ERROR: found non system-bus device as root device, ignored\n");
      return -1;
    }

  auto *f = IO_factory::get();

  // Register vbus to a cap with the same name if present
  L4::Cap<L4::Rcv_endpoint> cap =
    L4Re::Env::env()->get_cap<L4::Rcv_endpoint>(b->name());
  if (cap.is_valid())
    {
      cap = registry->register_obj(b, cap);
      if (!cap)
        {
          d_printf(DBG_ERR, "Service registration failed for vbus '%s' : %s\n",
                   b->name(), l4sys_errtostr(cap.cap()));
          return -1;
        }
      else
        d_printf(DBG_INFO, "Registered end point for vbus '%s'\n",
                 b->name());
    }
  else
    {
      // warn about missing end points if factory is not active
      if (!f->active())
        d_printf(DBG_WARN, "Service registration failed for vbus '%s', "
                 "IO did not get a cap with the corresponding name.\n",
                 b->name());
    }


  b->finalize();
  f->add_vbus(b);

  if (dlevel(DBG_DEBUG2))
    dump(b);
  return 0;
}


static int
read_config(char const *cfg_file, lua_State *lua)
{
  d_printf(DBG_INFO, "Loading: config '%s'\n", cfg_file);

  char const *lua_err = 0;
  int err = luaL_loadfile(lua, cfg_file);

  switch (err)
    {
    case 0:
      if ((err = lua_pcall(lua, 0, LUA_MULTRET, 0)))
        {
          lua_err = lua_tostring(lua, -1);
          d_printf(DBG_ERR, "%s: error executing lua config: %s\n", cfg_file, lua_err);
          lua_pop(lua, lua_gettop(lua));
          return -1;
        }
      lua_pop(lua, lua_gettop(lua));
      return 0;
      break;
    case LUA_ERRSYNTAX:
      lua_err = lua_tostring(lua, -1);
      lua_pop(lua, lua_gettop(lua));
      break;
    case LUA_ERRMEM:
      d_printf(DBG_ERR, "%s: out of memory while loading file\n", cfg_file);
      exit(1);
    case LUA_ERRFILE:
      lua_err = lua_tostring(lua, -1);
      d_printf(DBG_ERR, "%s: cannot open/read file: %s\n", cfg_file, lua_err);
      exit(1);
    default:
      d_printf(DBG_ERR, "%s: unknown error\n", cfg_file);
      exit(1);
    }

  d_printf(DBG_ERR, "%s: error using as lua config: %s\n", cfg_file, lua_err);
  return -1;
}



static int
arg_init(int argc, char * const *argv, Io_config_x *cfg)
{
  while (1)
    {
      int optidx = 0;
      int c;
      enum
      {
        OPT_TRANSPARENT_MSI   = 1,
        OPT_TRACE             = 2,
        OPT_ACPI_DEBUG        = 3,
      };

      struct option opts[] =
      {
        { "verbose",           0, 0, 'v' },
        { "transparent-msi",   0, 0, OPT_TRANSPARENT_MSI },
        { "trace",             1, 0, OPT_TRACE },
        { "acpi-debug-level",  1, 0, OPT_ACPI_DEBUG },
        { 0, 0, 0, 0 },
      };

      c = getopt_long(argc, argv, "v", opts, &optidx);
      if (c == -1)
        break;

      switch (c)
        {
        case 'v':
          cfg->inc_verbosity();
          break;
        case OPT_TRANSPARENT_MSI:
	  printf("Enabling transparent MSIs\n");
          cfg->set_transparent_msi(true);
          break;
        case OPT_TRACE:
          {
            unsigned trace_mask = strtol(optarg, 0, 0);
            set_trace_mask(trace_mask);
            printf("Set trace mask to 0x%08x\n", trace_mask);
            break;
          }
        case OPT_ACPI_DEBUG:
          {
            l4_uint32_t acpi_debug_level = strtol(optarg, 0, 0);
            acpi_set_debug_level(acpi_debug_level);
            printf("Set acpi debug level to 0x%08x\n", acpi_debug_level);
            break;
          }
        }
    }
  return optind;
}

class Dma_domain_phys : public Dma_domain
{
public:
  int set_managed_dma_space(std::shared_ptr<Managed_dma_space>,
                            Dma_domain_owner *) override
  { return -L4_EPERM; }

  int enumerate_dma_reservations(Dma_domain::Resv_cb) const override
  { return 0; }

  int get_dma_limits(l4_uint64_t *, l4_uint64_t *) const override
  { return -L4_EPERM; }

  bool needs_phys_dma() const override
  { return true; }

  bool has_iommu_dma() const override
  { return false; }
};

/**
 * DMA domain of a DMA capable device on a system with an IOMMU.
 *
 * A device not behind the IOMMU uses physical addresses. This is queried when
 * the DMA space is set: the IOMMU scopes may only be known after the device
 * enumeration.
 */
class Iommu_dma_domain : public Dma_domain
{
public:
  Iommu_dma_domain(Hw::Device *dev, Hw::Dma_src_feature *src)
  : _dev(dev), _src(src)
  {}

  bool needs_phys_dma() const override
  { return !has_iommu(); }

  bool has_iommu_dma() const override
  { return has_iommu(); }

  void warn_phys_dma() const override
  {
    if (!has_iommu())
      d_printf(DBG_WARN, "%s: DMA not behind the IOMMU, uses CPU-phys "
                         "addresses\n", _dev->name());
  }

  static void init()
  {
    _supports_remapping = true;
  }

  int set_managed_dma_space(std::shared_ptr<Managed_dma_space> space,
                            Dma_domain_owner *owner) override
  {
    if (int err = Dma_domain::set_managed_dma_space(space, owner); err < 0)
      return err;

    if (!has_iommu())
      return 0;

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    if (int err = _src->enumerate_dma_src_ids([this, iommu](l4_uint64_t src) -> int
                                              {
                                                return this->iommu_bind(iommu, src);
                                              });
        err < 0)
      {
        Dma_domain::clear_managed_dma_space(owner);
        return err;
      }

    return 0;
  }

  bool clear_managed_dma_space(Dma_domain_owner *owner) override
  {
    // Get hold of Managed_dma_space. The call below will clear it.
    auto mds = _managed_dma_space;

    if (!Dma_domain::clear_managed_dma_space(owner))
      return false;

    // set_managed_dma_space may not have been called in the first place
    if (!mds || !has_iommu())
      return true;

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    _src->enumerate_dma_src_ids([iommu, mds](l4_uint64_t src) -> int
                                {
                                  iommu_unbind(iommu, src, mds.get());
                                  return 0;
                                });

    return true;
  }

  int enumerate_dma_reservations(Dma_domain::Resv_cb cb) const override
  {
    if (!has_iommu())
      return 0;

    return _src->enumerate_dma_reservations(cb);
  }

  int get_dma_limits(l4_uint64_t *min_addr, l4_uint64_t *max_addr) const override
  {
    *min_addr = _min_addr;
    *max_addr = _max_addr;
    return 0;
  }

private:
  bool has_iommu() const
  { return _src && _src->has_iommu(); }

  int iommu_bind(L4::Cap<L4::Iommu> iommu, l4_uint64_t src)
  {
    l4_uint64_t min_addr;
    l4_uint64_t max_addr;
    int r = l4_error(iommu->bind(src, _managed_dma_space->dma_task(),
                                 &min_addr, &max_addr));
    if (r >= 0)
      {
        _min_addr = std::max(_min_addr, min_addr);
        _max_addr = std::min(_max_addr, max_addr);
      }
    else
      d_printf(DBG_ERR, "error: setting DMA for device: %d\n", r);

    return r;
  }

  static void iommu_unbind(L4::Cap<L4::Iommu> iommu, l4_uint64_t src,
                           Managed_dma_space *mds)
  {
    int r = l4_error(iommu->unbind(src, mds->dma_task()));
    if (r < 0)
      d_printf(DBG_ERR, "error: unbinding DMA for device: %d\n", r);
  }

  Hw::Device *_dev;
  Hw::Dma_src_feature *_src;
  l4_uint64_t _min_addr = 0;
  l4_uint64_t _max_addr = -1;
};

class Iommu_dma_domain_factory : public Hw::Dma_domain_factory
{
public:
  std::shared_ptr<Dma_domain> create(Hw::Device *dev) override
  {
    if (!dev)
      return nullptr;

    return std::make_shared<Iommu_dma_domain>(
      dev, dev->find_feature<Hw::Dma_src_feature>());
  }
};


static
int
run(int argc, char * const *argv)
{
  int argfileidx = arg_init(argc, argv, &_my_cfg);

  printf("Io service\n");
  set_debug_level(Io_config::cfg->verbose());

  d_printf(DBG_INFO, "Verboseness level: %d\n", Io_config::cfg->verbose());

  if (!L4Re::Env::env()->get_cap<L4Re::Dma_space_mgr>("dma_mgr"))
    {
      printf("ERROR: no 'dma_mgr' capability! This cap needs to be granted from moe.\n");
      exit(1);
    }

  res_init();

  if (dlevel(DBG_DEBUG))
    Phys_space::space.dump();

  L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
  if (!iommu || !iommu.validate().label())
    {
      d_printf(DBG_INFO, "no 'iommu' capability found, using CPU-phys for DMA\n");
      auto d = std::make_shared<Dma_domain_phys>();
      system_bus()->add_resource(d);
      system_bus()->set_downstream_dma_domain(d);
    }
  else
    {
      system_bus()->set_dma_domain_factory(
        std::make_unique<Iommu_dma_domain_factory>());
      Iommu_dma_domain::init();
    }

#if defined(ARCH_x86) || defined(ARCH_amd64)
  hw_system_bus()->set_can_alloc_cb([](Resource const *r)
                                        {
                                          if ((r->type() == Resource::Mmio_res)
                                              && (r->start() < (1UL << 20)))
                                            return false;

                                          return true;
                                        });

  //res_get_ioport(0xcf8, 4);
  res_get_ioport(0, 16);
#endif

  acpica_init();

  system_bus()->plugin();

  lua_State *lua = luaL_newstate();

  if (!lua)
    {
      printf("ERROR: cannot allocate Lua state\n");
      exit(1);
    }

  lua_newtable(lua);
  lua_setglobal(lua, "Io");

  for (int i = 0; libs[i].func; ++i)
    {
      luaL_requiref(lua, libs[i].name, libs[i].func, 1);
      lua_pop(lua, 1);
    }

  luaopen_Io(lua);

  extern char const _binary_io_lua_start[];
  extern char const _binary_io_lua_end[];

  if (luaL_loadbuffer(lua, _binary_io_lua_start,
                      _binary_io_lua_end - _binary_io_lua_start,
                      "@io.lua"))
    {
      d_printf(DBG_ERR, "INTERNAL: lua error: %s.\n", lua_tostring(lua, -1));
      lua_pop(lua, lua_gettop(lua));
      return 1;
    }

  if (lua_pcall(lua, 0, 1, 0))
    {
      d_printf(DBG_ERR, "INTERNAL: lua error: %s.\n", lua_tostring(lua, -1));
      lua_pop(lua, lua_gettop(lua));
      return 1;
    }

  for (; argfileidx < argc; ++argfileidx)
    read_config(argv[argfileidx], lua);

  acpi_late_setup();

  if (dlevel(DBG_DEBUG))
    {
      printf("Real Hardware -----------------------------------\n");
      dump(system_bus());
    }

  check_conflicts(system_bus());

  if (!registry->register_obj(platform_control(), "platform_ctl"))
    d_printf(DBG_WARN, "warning: could not register control interface at"
                       " cap 'platform_ctl'\n");

  fprintf(stderr, "Ready. Waiting for requests.\n");
  server_loop();

  return 0;
}

int
main(int argc, char * const *argv)
{
  try
    {
      return run(argc, argv);
    }
  catch (L4::Runtime_error &e)
    {
      fprintf(stderr, "FATAL uncaught exception: %s: %s\n"
                      "terminating...\n", e.str(), e.extra_str());
    }
  catch (...)
    {
      fprintf(stderr, "FATAL uncaught exception of unknown type\n"
                      "terminating...\n");
    }
  return -1;
}
