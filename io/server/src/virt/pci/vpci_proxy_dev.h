/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "vpci.h"

namespace Vi {

/**
 * Proxy PCI capability for PCI capability pass through.
 *
 * The passed-through capability has the identical offset in the config
 * space of the physical PCI device. The capability header is virtualized,
 * however the contents are forwarded to the physical device.
 */
class Pci_proxy_cap : public Pci_capability
{
private:
  Hw::Pci::If *_hwf;

public:

  /**
   * Make a pass-through capability.
   * \param hwf     The pysical PCI device.
   * \param offset  The config space offset of the capability.
   *
   * This constructor reads the physical PCI capability and provides
   * an equivalent PCI capability ID.
   */
  Pci_proxy_cap(Hw::Pci::If *hwf, l4_uint8_t offset)
  : Pci_capability(offset), _hwf(hwf)
  {
    set_id(_hwf->config().read<l4_uint8_t>(offset));
  }

  int cap_read(int offs, l4_uint32_t *v, Cfg_width order) override
  { return _hwf->cfg_read(offset() + offs, v, order); }

  int cap_write(int offs, l4_uint32_t v, Cfg_width order) override
  { return _hwf->cfg_write(offset() + offs, v, order); }
};

/**
 * Proxy extended PCI capability.
 */
class Pcie_proxy_cap : public Pcie_capability
{
private:
  Hw::Pci::If *_hwf;
  /// The physical offset in the physical config space.
  l4_uint16_t _phys_offset;

public:
  Pcie_proxy_cap(Hw::Pci::If *hwf, l4_uint32_t header,
                 l4_uint16_t offset, l4_uint16_t phys_offset)
  : Pcie_capability(offset), _hwf(hwf), _phys_offset(phys_offset)
  {
    set_cap(header);
  }

  int cap_read(int offs, l4_uint32_t *v, Cfg_width order) override
  { return _hwf->cfg_read(_phys_offset + offs, v, order); }

  int cap_write(int offs, l4_uint32_t v, Cfg_width order) override
  { return _hwf->cfg_write(_phys_offset + offs, v, order); }
};

/**
 * A virtual PCI proxy for a real PCI device.
 */
class Pci_proxy_dev : public Pci_dev_feature
{
public:
  Pci_proxy_dev(Hw::Pci::If *hwf);

  int cfg_read(int reg, l4_uint32_t *v, Cfg_width) override;
  int cfg_write(int reg, l4_uint32_t v, Cfg_width) override;
  int irq_enable(Irq_info *irq) override;

  l4_uint32_t read_rom() const { return _rom; }
  void write_rom(l4_uint32_t v);

  int vbus_dispatch(l4_umword_t, l4_uint32_t, L4::Ipc::Iostream &)
  { return -L4_ENOSYS; }

  Hw::Pci::If *hwf() const { return _hwf; }
  Msi_src *msi_src() const override;

  void dump() const;
  bool is_same_device(Pci_dev const *o) const override
  {
    if (Pci_proxy_dev const *op = dynamic_cast<Pci_proxy_dev const *>(o))
      return (hwf()->bus_nr() == op->hwf()->bus_nr())
             && (hwf()->device_nr() == op->hwf()->device_nr());
    return false;
  }

  bool match_hw_feature(const Hw::Dev_feature *f) const override
  { return f == _hwf; }

  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

  Pci_capability *find_pci_cap(unsigned offset) const;
  void add_pci_cap(Pci_capability *);
  Pcie_capability *find_pcie_cap(unsigned offset) const;
  void add_pcie_cap(Pcie_capability *);

  bool scan_pci_caps();
  void scan_pcie_caps();

private:
  Device *_host;
  Hw::Pci::If *_hwf;
  Pci_capability *_pci_caps = 0;
  Pcie_capability *_pcie_caps = 0;

  Pci::Bar_array<6> _vbars;
  l4_uint32_t _rom;

  l4_uint16_t _skip_pcie_cap(Hw::Pci::Extended_cap const &cap, unsigned sz);

  int _do_status_cmd_write(l4_uint32_t mask, l4_uint32_t value);
  void _do_cmd_write(unsigned mask,unsigned value);

  int _do_rom_bar_write(l4_uint32_t mask, l4_uint32_t value);
};

}
