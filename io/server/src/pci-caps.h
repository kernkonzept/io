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

/**
 * Empty base for a config space capability class.
 *
 * The main purpose of the class is to provide a Pci::Config
 * compatible base for specifying PCI config-space registers,
 * see `R`.
 */
struct Capability
{
  /**
   * Base class for defining a config-space register.
   *
   * \tparam OFS  The offset of the register in bytes relative to
   *              the PCI Capability.
   * \tparam T    Datatype of the config-space register. Note, this
   *              should be l4_uint8_t, l4_uint16_t, or l4_uint32_t.
   */
  template<unsigned OFS, typename T> struct R
  {
    enum { Ofs = OFS /**< Byteoffset of the register */ };
    T v; /**< Value container for the register */
  };
};

struct Pm_cap : Capability
{
  struct Pmc : R<0x02, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  2, version, v);
    CXX_BITFIELD_MEMBER( 3,  3, pme_clock, v);
    CXX_BITFIELD_MEMBER( 5,  5, dsi, v);
    CXX_BITFIELD_MEMBER( 6,  8, aux_current, v);
    CXX_BITFIELD_MEMBER( 9,  9, d1, v);
    CXX_BITFIELD_MEMBER(10, 10, d2, v);
    CXX_BITFIELD_MEMBER(11, 15, pme, v);
    CXX_BITFIELD_MEMBER(11, 11, pme_d0, v);
    CXX_BITFIELD_MEMBER(12, 12, pme_d1, v);
    CXX_BITFIELD_MEMBER(13, 13, pme_d2, v);
    CXX_BITFIELD_MEMBER(14, 14, pme_d3hot, v);
    CXX_BITFIELD_MEMBER(15, 15, pme_d3cold, v);
  };

  struct Pmcsr : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  2, state, v);
    CXX_BITFIELD_MEMBER( 3,  3, no_soft_reset, v);
    CXX_BITFIELD_MEMBER( 8,  8, pme_enable, v);
    CXX_BITFIELD_MEMBER( 9, 12, data_sel, v);
    CXX_BITFIELD_MEMBER(13, 14, data_scale, v);
    CXX_BITFIELD_MEMBER(15, 15, pme_status, v);
  };
};

} }

