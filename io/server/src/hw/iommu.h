/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "hw_device.h"

namespace Hw
{

class Iommu : public Hw::Device
{
public:
  Iommu()
  {
    add_cid("iommu");
    register_property("idx", &_idx);
  }

  void init() override
  {
    Hw::Device::init();

    if (_idx < 0)
      {
        d_printf(DBG_ERR, "error: %s: index must be positive\n", name());
        return;
      }
  }

  unsigned idx() const { return _idx; }

  l4_uint64_t translate_device_id(l4_uint64_t src)
  {
    /*
     * Fiasco src encoding:
     *   63-48: reserved
     *   47-32: iommu_idx
     *   31- 0: device_id
     */
    return (static_cast<l4_uint64_t>(_idx.val()) << 32) | src;
  }

private:
  Int_property _idx;
};

} // namespace Hw
