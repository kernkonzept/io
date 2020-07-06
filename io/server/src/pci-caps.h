/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <l4/cxx/bitfield>
#include <l4/sys/l4int.h>

namespace Hw { namespace Pci {

struct Pm_cap
{
  struct Pmc
  {
    l4_uint16_t raw;
    CXX_BITFIELD_MEMBER( 0,  2, version, raw);
    CXX_BITFIELD_MEMBER( 3,  3, pme_clock, raw);
    CXX_BITFIELD_MEMBER( 5,  5, dsi, raw);
    CXX_BITFIELD_MEMBER( 6,  8, aux_current, raw);
    CXX_BITFIELD_MEMBER( 9,  9, d1, raw);
    CXX_BITFIELD_MEMBER(10, 10, d2, raw);
    CXX_BITFIELD_MEMBER(11, 15, pme, raw);
    CXX_BITFIELD_MEMBER(11, 11, pme_d0, raw);
    CXX_BITFIELD_MEMBER(12, 12, pme_d1, raw);
    CXX_BITFIELD_MEMBER(13, 13, pme_d2, raw);
    CXX_BITFIELD_MEMBER(14, 14, pme_d3hot, raw);
    CXX_BITFIELD_MEMBER(15, 15, pme_d3cold, raw);
  };

  struct Pmcsr
  {
    l4_uint16_t raw;
    CXX_BITFIELD_MEMBER( 0,  2, state, raw);
    CXX_BITFIELD_MEMBER( 3,  3, no_soft_reset, raw);
    CXX_BITFIELD_MEMBER( 8,  8, pme_enable, raw);
    CXX_BITFIELD_MEMBER( 9, 12, data_sel, raw);
    CXX_BITFIELD_MEMBER(13, 14, data_scale, raw);
    CXX_BITFIELD_MEMBER(15, 15, pme_status, raw);
  };

  static l4_uint32_t pmc_reg(l4_uint32_t cap)
  { return cap + 2; }
};

} }

