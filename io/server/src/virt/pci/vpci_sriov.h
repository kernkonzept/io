/*
 * Copyright (C) 2020, 2024 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "vpci_proxy_dev.h"

#include <pci-caps.h>

namespace Vi {

class Sr_iov_proxy_cap : public Pcie_proxy_cap
{
private:
  using Sr_iov_cap = Hw::Pci::Sr_iov_cap;

public:
  Sr_iov_proxy_cap(Hw::Pci::If *hwf, l4_uint32_t header,
                   l4_uint16_t offset, l4_uint16_t phys_offset);

  int cap_read(l4_uint32_t offs, l4_uint32_t *v, Cfg_width order) override;
  int cap_write(l4_uint32_t offs, l4_uint32_t v, Cfg_width order) override;

private:
  bool _mse = true;
};

}
