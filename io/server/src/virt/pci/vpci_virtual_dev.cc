/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <pci-dev.h>

#include "vpci_virtual_dev.h"
#include "virt/vbus_factory.h"

namespace Vi {

int
Pci_virtual_dev::cfg_read(int reg, l4_uint32_t *v, Cfg_width order)
{
  reg >>= order;
  if ((unsigned)reg >= (_h_len >> order))
    return -L4_ERANGE;

#define RC(x) *v = *((Hw::Pci::Cfg_type<x>::Type const *)_h + reg); break
  *v = 0;
  switch (order)
    {
    case Hw::Pci::Cfg_byte: RC(Hw::Pci::Cfg_byte);
    case Hw::Pci::Cfg_short: RC(Hw::Pci::Cfg_short);
    case Hw::Pci::Cfg_long: RC(Hw::Pci::Cfg_long);
    }

  return 0;
#undef RC
}

int
Pci_virtual_dev::cfg_write(int reg, l4_uint32_t v, Cfg_width order)
{
  switch (reg & ~3)
    {
    case 0x4: // status is RO
      v &= 0x0000ffff << (reg & (3 >> order));
      break;

    default:
      break;
    }

  reg >>= order;
  if ((unsigned)reg >= (_h_len >> order))
    return -L4_ERANGE;

#define RC(x) *((Hw::Pci::Cfg_type<x>::Type *)_h + reg) = v; break
  switch (order)
    {
    case Hw::Pci::Cfg_byte: RC(Hw::Pci::Cfg_byte);
    case Hw::Pci::Cfg_short: RC(Hw::Pci::Cfg_short);
    case Hw::Pci::Cfg_long: RC(Hw::Pci::Cfg_long);
    }

  return 0;
#undef RC
}

Pci_dummy::Pci_dummy()
{
  add_feature(this);
  memset(_cfg_space, 0, sizeof(_cfg_space));

  _h = &_cfg_space[0];
  _h_len = sizeof(_cfg_space);

  cfg_hdr()->hdr_type = 0x80;
  cfg_hdr()->vendor_device = 0x02000400;
  cfg_hdr()->status = 0;
  cfg_hdr()->class_rev = 0x36440000;
  cfg_hdr()->cmd = 0x0;
}

namespace {
  static Dev_factory_t<Pci_dummy> __pci_dummy_factory("PCI_dummy_device");
}

}
