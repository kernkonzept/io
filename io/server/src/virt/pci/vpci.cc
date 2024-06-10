/*
 * (c) 2010-2020 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/vbus/vbus_pci-ops.h>

#include <l4/sys/err.h>

#include "vpci.h"

namespace Vi {

int
Pci_dev_feature::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream& ios)
{
  if (l4vbus_subinterface(func) != L4VBUS_INTERFACE_PCIDEV)
    return -L4_ENOSYS;

  l4_uint32_t reg;
  l4_uint32_t value = 0;
  l4_uint32_t width;
  Pci_dev::Irq_info info;
  int res;

  switch (func)
    {
    case L4vbus_pcidev_cfg_read:
      ios >> reg >> width;
      res = cfg_read(reg, &value, Hw::Pci::cfg_w_to_o(width));
      if (res < 0)
        return res;
      ios << value;
      return L4_EOK;
    case L4vbus_pcidev_cfg_write:
      ios >> reg >> value >> width;
      return cfg_write(reg, value, Hw::Pci::cfg_w_to_o(width));
    case L4vbus_pcidev_cfg_irq_enable:
      res = irq_enable(&info);
      if (res < 0)
        return res;
      ios << info.irq << info.trigger << info.polarity;
      return L4_EOK;
    default: return -L4_ENOSYS;
    }
}

}
