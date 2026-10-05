/*
 * Copyright (C) 2019-2020, 2022, 2024-2025 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <pci-dev.h>
#include <pci-caps.h>

#include "cfg.h"
#include "debug.h"
#include "hw/hw_msi.h"
#include "main.h"

namespace {

class Saved_acs_cap : public Hw::Pci::Saved_cap
{
public:
  explicit Saved_acs_cap(unsigned offset)
  : Hw::Pci::Saved_cap(Hw::Pci::Extended_cap::Acs, offset) {}

private:
  enum Regs
  {
    Capabilities = 0x4,
    Control      = 0x6,
  };

  l4_uint16_t control;

  void _save(Hw::Pci::Config cap) override
  {
    cap.read(Control, &control);
  }

  void _restore(Hw::Pci::Config cap) override
  {
    cap.write(Control, control);
  }
};

}

bool
Hw::Pci::Dev::handle_acs_cap(Hw::Pci::Dev *dev, Hw::Pci::Extended_cap acs_cap)
{
  auto caps = acs_cap.read<Hw::Pci::Acs_cap::Caps>();
  auto ctrl = acs_cap.read<Hw::Pci::Acs_cap::Ctrl>();

  // always disable egress control and direct translated P2P
  ctrl.p2p_egress_ctrl_enable() = 0;
  ctrl.direct_translated_p2p_enable() = 0;

  // enable all other supported ACS features
  ctrl.f() = caps.f();

  acs_cap.write(ctrl);

  // Certain Intel PCIe root ports implement ACS but instead of using words
  // for the capability and control registers they use dwords. This means the
  // control register is at offset 8 instead of 6. So we hopefully tried to
  // write into an RO area.
  // We read the control word and compare it with our desired configuration.
  // If they don't match we print a warning.
  auto c = acs_cap.read<Hw::Pci::Acs_cap::Ctrl>();

  if (c.enabled() != ctrl.enabled())
    {
      d_printf(DBG_ERR,
               "Error: PCI ACS control does not match desired configuration. "
               "Is this a buggy PCIe root port?\n");
      return false;
    }

  printf("ACS: %02x:%02x.%x: enabled ACS (features=0x%x)\n",
         dev->bus_nr(), dev->device_nr(), dev->function_nr(),
         ctrl.enabled().get());

  if (c.upstream_fwd_enable())
    dev->_acs_upstream_fwd = true; // root ports have this enabled implicitly

  if (c.p2p_request_redirect_enable() && c.p2p_completion_redirect_enable())
    dev->_acs_p2p_redirect = true;

  dev->add_saved_cap(new Saved_acs_cap(acs_cap.reg()));

  return true;
}
