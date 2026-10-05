/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "pci-dev.h"
#include "pci-caps.h"

bool
Hw::Pci::Dev::handle_ari_cap(Hw::Pci::Dev *dev, Hw::Pci::Extended_cap)
{
  // This device is an ARI device, if it is immediately below a Downstream Port
  // (devfn == 0) try to enable ARI forwarding.
  if (dev->devfn() == 0)
    dev->bridge()->enable_ari_forwarding();

  return true;
}
