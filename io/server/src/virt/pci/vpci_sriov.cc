/*
 * Copyright (C) 2020, 2024 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "vpci_sriov.h"

#include "reg_matcher.h"
#include "virt/vbus_factory.h"

#include <pci-sriov.h>

#include <cassert>
#include <cstdio>

namespace Vi {

Sr_iov_proxy_cap::Sr_iov_proxy_cap(Hw::Pci::If *hwf, l4_uint32_t header,
                                   l4_uint16_t offset, l4_uint16_t phys_offset)
: Pcie_proxy_cap(hwf, header, offset, phys_offset)
{
  d_printf(DBG_INFO, "SR-IOV proxy cap: created\n");
  set_size(0x40);
}

int Sr_iov_proxy_cap::cap_read(l4_uint32_t offs, l4_uint32_t *v, Cfg_width order)
{
  d_printf(DBG_DEBUG2, "SR-IOV proxy cap: read %x, w=%d\n", offs, order);

  Reg_matcher m{offs, order};
  if (m.is_reg<Sr_iov_cap::Ctrl>())
    {
      int r = Pcie_proxy_cap::cap_read(offs, v, order);
      if (!_mse)
        *v &= ~Sr_iov_cap::Ctrl::vf_memory_enable_bfm_t::Mask;
      return r;
    }
  else if (m.in_range<Sr_iov_cap::Vf_bar0, Sr_iov_cap::Vf_bar5>())
    {
      // Hide VBARs so that driver/helper VM does not attempt to reconfigure
      // them.
      *v = 0;
      return 0;
    }
  else if (m.is_reg<Sr_iov_cap::Fn_dep>())
    {
      // Fcn Dep Link makes no sense for the vbus interface which IO uses to
      // expose PCI devices. Always return zero.
      *v = 0;
      return 0;
    }
  else if (m.invalid_access())
    {
      // Ignore invalid reads.
      *v = 0;
      return 0;
    }
  else
    return Pcie_proxy_cap::cap_read(offs, v, order);
}

int Sr_iov_proxy_cap::cap_write(l4_uint32_t offs, l4_uint32_t v, Cfg_width order)
{
  d_printf(DBG_DEBUG2, "SR-IOV proxy cap: write r=0x%x, v=0x%x, w=%d...\n", offs, v, order);

  Reg_matcher m{offs, order};
  if (m.is_reg<Sr_iov_cap::Ctrl>())
    {
      bool mse = !!(v & Sr_iov_cap::Ctrl::vf_memory_enable_bfm_t::Mask);
      _mse = mse;
      return 0;
    }
  else if (m.is_reg<Sr_iov_cap::Vf_migration_state>())
    return Pcie_proxy_cap::cap_write(offs, v, order);
  else // Ignore invalid and non-matched writes.
    return 0;
}


class Sr_iov_proxy_dev : public Pci_proxy_dev
{
public:
  explicit Sr_iov_proxy_dev(Hw::Pci::Sr_iov_vf *hwf) : Pci_proxy_dev(hwf)
  {
    d_printf(DBG_INFO, "Create Vi::Sr_iov_proxy_dev[%p] for %p %s\n", this, hwf, typeid(*hwf).name());
  }
};

namespace {
  static Feature_factory_t<Sr_iov_proxy_dev, Hw::Pci::Sr_iov_vf> __sriov_f_f;
}

}
