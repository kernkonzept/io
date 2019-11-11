// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#include "pci.h"

#include "cfg.h"
#include "debug.h"
#include "hw_msi.h"
#include "main.h"

namespace Hw { namespace Pci {

namespace {

namespace Acs
{
  enum Regs
  {
    Capabilities = 0x4,
    Control      = 0x6,
  };

  enum Ctrl_enable_bits
  {
    Src_validation       = 1 << 0,
    Translation_blocking = 1 << 1,
    Request_redirect     = 1 << 2,
    Completion_redirect  = 1 << 3,
    Upstream_forwarding  = 1 << 4,
    Egress_ctrl          = 1 << 5,
    Direct_p2p           = 1 << 6,
  };
}

class Saved_acs_cap : public Saved_cap
{
public:
  explicit Saved_acs_cap(unsigned offset) : Saved_cap(Extended_cap::Acs, offset) {}

private:
  l4_uint16_t control;

  void _save(Cfg_ptr cap) override
  {
    cap.read(Acs::Control, &control);
  }

  void _restore(Cfg_ptr cap) override
  {
    cap.write(Acs::Control, control);
  }
};

}


void
Dev::parse_acs_cap(Extended_cap acs_cap)
{
  l4_uint16_t caps;
  acs_cap.read(Acs::Capabilities, &caps);
  d_printf(DBG_DEBUG, "ACS: %02x:%02x.%x: enable ACS, capabilities: %x\n",
           bus_nr(), device_nr(), function_nr(), caps);

  // always disable egress control and direct translated P2P
  caps &= 0x7f & ~(Acs::Egress_ctrl | Acs::Direct_p2p);
  // enable all other ACS features if supported
  caps &=   Acs::Src_validation | Acs::Translation_blocking
          | Acs::Request_redirect | Acs::Completion_redirect
          | Acs::Upstream_forwarding;

  acs_cap.write(Acs::Control, caps);

  _saved_state.add_cap(new Saved_acs_cap(acs_cap.reg()));
}

}}
