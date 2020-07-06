/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <pci-dev.h>
#include <pci-if.h>

namespace Hw { namespace Pci {

class Bridge_base : public Bridge_if
{
public:
  unsigned char num = 0;
  unsigned char subordinate = 0;

  Bridge_base() = default;

  explicit Bridge_base(unsigned char num)
  : num(num), subordinate(num)
  {}

  bool check_bus_number(unsigned bus_num) override
  {
    return bus_num <= subordinate;
  }

  virtual ~Bridge_base() = default;

  void discover_bus(Hw::Device *host, Config_space *cfg,
                    Io_irq_pin::Msi_src *ext_msi = nullptr);
  void dump(int) const;

protected:
  virtual void discover_devices(Hw::Device *host, Config_space *cfg,
                                Io_irq_pin::Msi_src *ext_msi);
  void discover_device(Hw::Device *host_bus, Config_space *cfg,
                       Io_irq_pin::Msi_src *ext_msi, int devnum);
  Dev *discover_func(Hw::Device *host_bus, Config_space *cfg,
                     Io_irq_pin::Msi_src *ext_msi,
                     int devnum, int func);
};

class Irq_router : public Resource
{
public:
  Irq_router() : Resource(Irq_res) {}
  void dump(int) const override;
  bool compatible(Resource *consumer, bool = true) const override
  {
    // only relative CPU IRQ lines are compatible with IRQ routing
    // global IRQs must be allocated at a higher level
    return consumer->type() == Irq_res && consumer->flags() & F_relative;
  }

};

template< typename RES_SPACE >
class Irq_router_res : public Irq_router
{
protected:
  typedef RES_SPACE Irq_rs;
  mutable Irq_rs _rs;

public:
  template< typename ...ARGS >
  Irq_router_res(ARGS && ...args) : _rs(cxx::forward<ARGS>(args)...) {}

  RES_SPACE *provided() const override { return &_rs; }
};


/**
 * Default router if no IRQ router is present on the PCI bus.
 *
 * This router forwards Resource_space::request() calls to the parent bus. The
 * mapping from the IRQ pin (\#IRQA..\#IRQD) to the interrupt line depends on
 * the device slot.
 */
class Pci_pci_bridge_irq_router_rs : public Resource_space
{
public:
  bool request(Resource *parent, ::Device *,
               Resource *child, ::Device *cdev) override;
  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Pci_pci_bridge_irq_router_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Pci_pci_bridge_irq_router_rs\n");
    return false;
  }
};

class Pci_pci_bridge_basic : public Bridge_base, public Dev
{
public:
  unsigned char pri;
  using Dev::cfg_write;
  using Dev::cfg_read;

  /**
   * Constructor to create a new Pci_pci_bridge_basic object
   *
   * \param[in] host      Parent device
   * \param[in] bus       PCI bus
   * \param     hdr_type  Header type that defines the layout of the PCI config
   *                      header.
   */
  Pci_pci_bridge_basic(Hw::Device *host, Bridge_if *bridge,
                       Io_irq_pin::Msi_src *ext_msi,
                       Config_cache const &cfg)
  : Dev(host, bridge, ext_msi, cfg), pri(0)
  {}

  unsigned alloc_bus_number() override
  {
    unsigned n = _bridge->alloc_bus_number();
    if (n == 0)
      return 0;

    subordinate = n;
    config().write<l4_uint8_t>(Config::Subordinate, n);
    return n;
  }

  virtual void check_bus_config();

  void discover_bus(Hw::Device *host) override
  {
    Bridge_base::discover_bus(host, cfg.cfg_spc(), _external_msi_src);
    Dev::discover_bus(host);
  }

  void dump(int indent) const override
  {
    Dev::dump(indent);
    Bridge_base::dump(indent);
  }

};

class Pci_pci_bridge : public Pci_pci_bridge_basic
{
public:
  Resource *mmio;
  Resource *pref_mmio;
  Resource *io;

  explicit Pci_pci_bridge(Hw::Device *host, Bridge_if *bridge,
                          Io_irq_pin::Msi_src *ext_msi,
                          Config_cache const &cfg)
  : Pci_pci_bridge_basic(host, bridge, ext_msi, cfg), mmio(0), pref_mmio(0), io(0)
  {}

  void setup_children(Hw::Device *host) override;
  void discover_resources(Hw::Device *host) override;
};

} }
