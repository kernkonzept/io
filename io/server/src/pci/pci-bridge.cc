/*
 * Copyright (C) 2010-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include <pci-bridge.h>
#include <pci-caps.h>
#include <pci-driver.h>
#include <resource_provider.h>

namespace Hw { namespace Pci {

namespace {

static inline l4_uint32_t
devfn(unsigned dev, unsigned fn)
{ return (dev << 16) | fn; }

}

void
Irq_router::dump(int indent) const
{
  printf("%*sPCI IRQ ROUTER: %s (%p)\n", indent, "", typeid(*this).name(),
         this);
}

bool
Pci_pci_bridge_irq_router_rs::request(Resource *parent, ::Device *pdev,
                                      Resource *child, ::Device *cdev)
{
  bool res = false;

  Hw::Device *cd = dynamic_cast<Hw::Device*>(cdev);

  if (!cd)
    return res;

  if (pdev->parent())
    {
      child->start((child->start() + (cd->adr() >> 16)) & 3);
      res = pdev->parent()->request_child_resource(child, pdev);
      if (res)
        child->parent(parent);
    }

  return res;
}

void
Generic_bridge::check_bus_config()
{
  auto c = config();

  // the config space address for bus config is the same in
  // type 1 and type 2
  l4_uint32_t b = c.read<l4_uint32_t>(Config::Primary);

  l4_uint8_t pb = b & 0xff;
  l4_uint8_t sb = (b >> 8) & 0xff;
  l4_uint8_t so = (b >> 16) & 0xff;

  // set internal attributes
  pri = pb;
  num = sb;
  subordinate = so;

  if (pb == cfg.addr().bus() && sb > cfg.addr().bus()
      && _bridge->check_bus_number(sb))
    return; // nothing to do primary and secondary bus numbers are sane.

  unsigned new_so = _bridge->alloc_bus_number();
  if (new_so == 0)
    assert (false);

  pri = cfg.addr().bus();
  num = new_so;
  subordinate = new_so;
  b &= 0xff000000;
  b |= static_cast<l4_uint32_t>(pri);
  b |= static_cast<l4_uint32_t>(num) << 8;
  b |= static_cast<l4_uint32_t>(subordinate) << 16;

  // Write Config::Primary, Config::Secondary and Config::Subordinate.
  c.write(Config::Primary, b);
}

int
Generic_bridge::enumerate_dma_src_ids(Dma_src_feature::Dma_src_id_cb cb) const
{
  if (auto *parent = dynamic_cast<Dma_src_feature*>(parent_bridge()))
    if (int ret = parent->enumerate_dma_src_ids(cb))
      return ret;

  Dma_requester_id alias = dma_alias();
  if (!alias)
    return 0;

  l4_uint64_t si = 0;
  int ret = translate_dma_src(alias, &si);
  if (ret < 0)
    return ret;

  ret = cb(si);
  if (ret < 0)
    return ret;

  // Stop emitting DMA source IDs if the bridge takes ownership of all
  // transactions.
  return alias.is_rewrite() ? 1 : 0;
}

void
Bridge::setup_children(Hw::Device *)
{
  auto c = config();
  if (!mmio->empty() && mmio->valid())
    {
      l4_uint32_t v = (mmio->start() >> 16) & 0xfff0;
      v |= mmio->end() & 0xfff00000;
      c.write<l4_uint32_t>(Config::Mem_base, v);
      if (0)
        printf("%08x: set mmio to %08x\n", host()->adr(), v);
      if (0)
        printf("%08x: mmio =      %08x\n", host()->adr(), c.read<l4_uint32_t>(0x20));

      c.write<l4_uint16_t>(Config::Command, c.read<l4_uint16_t>(0x04) | 3);
    }

  if (!pref_mmio->empty() && pref_mmio->valid())
    {
      l4_uint32_t v = (pref_mmio->start() >> 16) & 0xfff0;
      v |= pref_mmio->end() & 0xfff00000;
      c.write<l4_uint32_t>(Config::Pref_mem_base, v);
      if (0)
        printf("%08x: set pref mmio to %08x\n", host()->adr(), v);
    }

  enable_bus_master();

  // Enable forwarding of secondary interface SERR# assertions.
  l4_uint16_t v = c.read<l4_uint16_t>(Config::Bridge_control);
  c.write<l4_uint16_t>(Config::Bridge_control, (v | 0x2));
}

void
Bridge::discover_resources(Hw::Device *host)
{
  if (flags.discovered())
    return;

  auto c = config();

  l4_uint32_t v;
  l4_uint64_t s, e;

  v = c.read<l4_uint32_t>(Config::Mem_base);
  s = (v & 0xfff0) << 16;
  e = (v & 0xfff00000) | 0xfffff;

  Resource *r = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw
                                      | Resource::F_can_move
                                      | Resource::F_can_resize);
  r->set_id("WIN0");
  r->alignment(0xfffff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();

  if (0)
    printf("%08x: mmio = %08x\n", _host->adr(), v);
  mmio = r;
  mmio->validate();
  _host->add_resource_rq(mmio);

  r = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw
                            | Resource::F_prefetchable
                            | Resource::F_can_move | Resource::F_can_resize);
  r->set_id("WIN1");
  v = c.read<l4_uint32_t>(Config::Pref_mem_base);
  s = (v & 0xfff0) << 16;
  e = (v & 0xfff00000) | 0xfffff;

  if ((v & 0x0f) == 1)
    {
      r->add_flags(Resource::F_width_64bit);
      v = c.read<l4_uint32_t>(Config::Pref_mem_base_hi);
      s |= l4_uint64_t(v) << 32;
      v = c.read<l4_uint32_t>(Config::Pref_mem_limit_hi);
      e |= l4_uint64_t(v) << 32;
    }

  r->alignment(0xfffff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();

  pref_mmio = r;
  r->validate();
  _host->add_resource_rq(r);

  v = c.read<l4_uint16_t>(Config::Io_base);
  s = (v & 0xf0) << 8;
  e = (v & 0xf000) | 0xfff;

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN2");
  r->alignment(0xfff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();
  io = r;
  r->validate();
  _host->add_resource_rq(r);

  Dev::discover_resources(host);
}

Hw::Pci::Dma_requester_id
Bridge::dma_alias() const
{
  // Legacy PCI bridges take ownership of DMA transactions
  return Dma_requester_id::rewrite(segment_nr(), bus_nr(), devfn());
}

class Pcie_downstream_port : public Bridge
{
private:
  bool _ari = false;

public:
  explicit Pcie_downstream_port(Hw::Device *host, Bridge_if *bridge,
                                Config_cache const &cfg)
  : Bridge(host, bridge, cfg)
  {
  }

  bool ari_forwarding_enable() override
  {
    if (_ari)
      return true;

    Cap pcie = pcie_cap();
    auto dev_caps2 = pcie.read<Pcie_cap::Dev_caps2>();
    if (dev_caps2.ari_forwarding_supported())
      {
        auto dev_ctrl2 = pcie.read<Pcie_cap::Dev_ctrl2>();
        dev_ctrl2.ari_forwarding_enable() = true;
        pcie.write(dev_ctrl2);

        _ari = true;
      }
    return _ari;
  }

  void discover_devices(Hw::Device *host_bus, Config_space *cfg) override
  {
    Dev *d = discover_func(host_bus, cfg, 0, 0);
    if (!d)
      return;

    // look for functions in PCI style
    if (d->cfg.is_multi_function())
      for (int function = 1; function < 8; ++function)
        discover_func(host_bus, cfg, 0, function);
  }

  Dma_requester_id dma_alias() const override
  { return Dma_requester_id(); }
};

class Pcie_upstream_port : public Bridge
{
public:
  explicit Pcie_upstream_port(Hw::Device *host, Bridge_if *bridge,
                              Config_cache const &cfg)
  : Bridge(host, bridge, cfg)
  {}

  Dma_requester_id dma_alias() const override
  { return Dma_requester_id(); }
};

class Pcie_bridge : public Bridge
{
public:
  explicit Pcie_bridge(Hw::Device *host, Bridge_if *bridge,
                       Config_cache const &cfg)
  : Bridge(host, bridge, cfg)
  {}

  Dma_requester_id dma_alias() const override
  {
    /*
     * PCIe-to-PCI(-X) bridges alias _some_ transactions with their secondary
     * bus number. See PCI Express to PCI/PCI-X Bridge Specification Revision
     * 1.0, section 2.3.
     */
    return Dma_requester_id::alias(segment_nr(), num, 0);
  }
};

class Cardbus_bridge : public Generic_bridge
{
public:
  Cardbus_bridge(Hw::Device *host, Bridge_if *bridge,
                 Config_cache const &cfg)
  : Generic_bridge(host, bridge, cfg)
  {}

  void discover_resources(Hw::Device *host) override;

  Dma_requester_id dma_alias() const override
  {
    // TODO: does the cardbus bridge really take ownership?
    return Dma_requester_id::rewrite(segment_nr(), bus_nr(), devfn());
  }
};

void
Cardbus_bridge::discover_resources(Hw::Device *host)
{
  if (flags.discovered())
    return;

  auto c = config();

  Resource *r = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw
                                      | Resource::F_can_move
                                      | Resource::F_can_resize);
  r->set_id("WIN0");
  r->start(c.read<l4_uint32_t>(Config::Cb_mem_base_0));
  r->end(c.read<l4_uint32_t>(Config::Cb_mem_limit_0));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw
                            | Resource::F_can_move | Resource::F_can_resize);
  r->set_id("WIN1");
  r->start(c.read<l4_uint32_t>(Config::Cb_mem_base_1));
  r->end(c.read<l4_uint32_t>(Config::Cb_mem_limit_1));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN2");
  r->start(c.read<l4_uint32_t>(Config::Cb_io_base_0));
  r->end(c.read<l4_uint32_t>(Config::Cb_io_limit_0));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN3");
  r->start(c.read<l4_uint32_t>(Config::Cb_io_base_1));
  r->end(c.read<l4_uint32_t>(Config::Cb_io_limit_1));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  flags.discovered() = true;
}


static Generic_bridge *
create_pci_pci_bridge(Bridge_if *bridge,
                      Config const &,
                      Config_cache const &cc,
                      Hw::Device *hw)
{
  if (cc.type() != 1)
    {
      d_printf(DBG_WARN,
               "ignoring PCI-PCI bridge with invalid header type: %u (%08x)\n",
               (unsigned)cc.type(), hw->adr());
      return nullptr;
    }

  Generic_bridge *b = nullptr;
  if (cc.pcie_cap)
    {
      switch (cc.pcie_type)
        {
        case 0x4: // Root Port of PCI Express Root Complex
        case 0x6: // Downstream Port of PCI Express Switch
          b = new Pcie_downstream_port(hw, bridge, cc);
          break;

        case 0x5: // Upstream Port of PCI Express Switch
          b = new Pcie_upstream_port(hw, bridge, cc);
          break;

        case 0x7: // PCI Express to PCI/PCI-X bridge
          b = new Pcie_bridge(hw, bridge, cc);
          break;

        default:
          // all other ids are either no busses or
          // legacy PCI/PCI-X busses
          b = new Bridge(hw, bridge, cc);
          break;
        }
    }
  else
    b = new Bridge(hw, bridge, cc);

  b->check_bus_config();
  hw->set_name_if_empty("PCI-to-PCI bridge");
  return b;
}

static Generic_bridge *
create_pci_cardbus_bridge(Bridge_if *bridge,
                          Config const &,
                          Config_cache const &cc,
                          Hw::Device *hw)
{
  if (cc.type() != 2)
    {
      d_printf(DBG_WARN,
               "ignoring PCI-Cardbus bridge with invalid header type: %u (%08x)\n",
               (unsigned)cc.type(), hw->adr());
      return nullptr;
    }

  hw->set_name_if_empty("PCI-to-Cardbus bridge");
  auto b = new Cardbus_bridge(hw, bridge, cc);
  b->check_bus_config();
  return b;
}

static Dev *
create_pci_bridge(Bridge_if *bridge,
                  Config const &cfg,
                  Config_cache const &cc,
                  Hw::Device *hw)
{
  switch (cc.sub_class())
    {
    case 0x4: return create_pci_pci_bridge(bridge, cfg, cc, hw);
    case 0x7: return create_pci_cardbus_bridge(bridge, cfg, cc, hw);
    default:
      if (cc.type() != 0)
        {
          d_printf(DBG_WARN,
                   "ignoring PCI bridge with invalid header type: %u (%08x)\n",
                   (unsigned)cc.type(), hw->adr());
          return nullptr;
        }

      hw->set_name_if_empty("PCI device");
      return new Dev(hw, bridge, cc);
    }
}

void
Bridge_base::discover_device(Hw::Device *host_bus, Config_space *cfg,
                             int devnum)
{
  Dev *d = discover_func(host_bus, cfg, devnum, 0);
  if (!d)
    return;

  // look for functions in PCI style
  if (d->cfg.is_multi_function())
    for (int function = 1; function < 8; ++function)
      discover_func(host_bus, cfg, devnum, function);
}

Dev *
Bridge_base::discover_func(Hw::Device *host_bus, Config_space *cfg,
                           int device, int function)
{
  Config config(Cfg_addr(num, device, function, 0), cfg);

  l4_uint32_t vendor = config.read<l4_uint32_t>(Config::Vendor);
  if ((vendor & 0xffff) == 0xffff)
    return nullptr;

#if 0
  // alex: hack disable serial IO cards for user apps
  if ((_class >> 16) == 0x0700)
    continue;
  // alex: hack end
#endif

  Hw::Device *child = host_bus->get_child_dev_adr(devfn(device, function), true);

  // skip device if we already were here
  if (Dev *dev = child->find_feature<Dev>())
    return dev;

  Config_cache cc;
  cc.fill(vendor, config);

  Dev *d;
  if (cc.base_class() == 0x6) // bridge
    {
      d = create_pci_bridge(this, config, cc, child);
      if (!d)
        return nullptr;
    }
  else
    {
      child->set_name_if_empty("PCI device");
      d = new Dev(child, this, cc);
    }

  child->add_feature(d);

  // discover the resources of the new PCI device
  // NOTE: we do this here to have all child resources discovered and
  // requested before the call to allocate_pending_resources in
  // hw_device.cc which can then recursively try to allocate resources
  // that were not preset
  d->discover_resources(child);

  Driver *drv = Driver::find(d);
  if (drv)
    drv->probe(d);

  // go down the PCI hierarchy recursively,
  // to assign bus numbers (if not yet assigned) the right way
  d->discover_bus(child);

  return d;
}

void
Bridge_base::discover_bus(Hw::Device *host, Config_space *cfg)
{
  Resource *r = host->resources()->find_if(Resource::is_irq_provider_s);

  if (!r)
    {
      r = new Pci::Irq_router_res<Pci::Pci_pci_bridge_irq_router_rs>();
      r->set_id("IRQR");
      host->add_resource_rq(r);
    }

  discover_devices(host, cfg);
}

void
Bridge_base::discover_devices(Hw::Device *host, Config_space *cfg)
{
  for (int device = 0; device <= 31; ++device)
    discover_device(host, cfg, device);
}

void
Bridge_base::dump(int) const
{
#if 0
  printf("PCI bus type: %d: ", bus_type);
  "bridge %04x:%02x:%02x.%x\n",
         b->num, 0, b->parent() ? (int)static_cast<Hw::Pci::Bus*>(b->parent())->num : 0,
         b->adr() >> 16, b->adr() & 0xffff);
#endif
#if 0
  //dump_res_rec(resources(), 0);

  for (iterator c = begin(0); c != end(); ++c)
    c->dump();
#endif
};

} }

