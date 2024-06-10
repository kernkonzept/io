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
 * A basic really virtualized PCI device.
 */
class Pci_virtual_dev : public Pci_dev_feature
{
public:
  struct Pci_cfg_header
  {
    l4_uint32_t vendor_device;
    l4_uint16_t cmd;
    l4_uint16_t status;
    l4_uint32_t class_rev;
    l4_uint8_t  cls;
    l4_uint8_t  lat;
    l4_uint8_t  hdr_type;
    l4_uint8_t  bist;
  } __attribute__((packed));

  Pci_cfg_header *cfg_hdr() { return (Pci_cfg_header*)_h; }
  Pci_cfg_header const *cfg_hdr() const { return (Pci_cfg_header const *)_h; }

  int cfg_read(int reg, l4_uint32_t *v, Cfg_width) override;
  int cfg_write(int reg, l4_uint32_t v, Cfg_width) override;

  bool is_same_device(Pci_dev const *o) const override
  { return o == this; }

  Io_irq_pin::Msi_src *msi_src() const override
  { return 0; }

  ~Pci_virtual_dev() = 0;

  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

protected:
  Device *_host;
  unsigned char *_h;
  unsigned _h_len;

};

inline
Pci_virtual_dev::~Pci_virtual_dev() = default;

/**
 * Virtual PCI dummy device
 */
class Pci_dummy : public Pci_virtual_dev, public Device
{
private:
  unsigned char _cfg_space[4*4];

public:
  int irq_enable(Irq_info *irq) override
  {
    irq->irq = -1;
    return -1;
  }

  Pci_dummy();

  bool match_hw_feature(const Hw::Dev_feature*) const override { return false; }
  void set_host(Device *d) override { _host = d; }
  Device *host() const override { return _host; }

private:
  Device *_host;
};

}
