/*
 * Copyright (C) 2010-2020, 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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
   *              the PCI capability.
   * \tparam T    Datatype of the config-space register. Note, this
   *              should be l4_uint8_t, l4_uint16_t, or l4_uint32_t.
   */
  template<unsigned OFS, typename T> struct R
  {
    enum { Ofs = OFS /**< Offset of the register in bytes */ };
    T v; /**< Value container for the register */
  };
};

struct Pcie_cap : Capability
{
  struct Dev_caps2 : R<0x24, l4_uint32_t>
  {
    CXX_BITFIELD_MEMBER( 5,  5, ari_forwarding_supported, v);
  };

  struct Dev_ctrl2 : R<0x28, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 5,  5, ari_forwarding_enable, v);
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

struct Sr_iov_cap : Capability
{
  enum
  {
    Id   = 0x10,
    Size = 0x40,
  };

  struct Caps : R<4, l4_uint32_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_migration, v);
    CXX_BITFIELD_MEMBER( 1,  1, ari_preserved, v);
    CXX_BITFIELD_MEMBER(21, 31, vf_migration_msg, v);
  };

  struct Ctrl : R<8, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_enable, v);
    CXX_BITFIELD_MEMBER( 1,  1, vf_migration_enable, v);
    CXX_BITFIELD_MEMBER( 2,  2, vf_migration_irq_enable, v);
    CXX_BITFIELD_MEMBER( 3,  3, vf_memory_enable, v);
    CXX_BITFIELD_MEMBER( 4,  4, ari_capable_hierarchy, v);
  };

  struct Status : R<0xA, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, vf_migartion_status, v);
  };

  using Initial_vfs  = R<0x0C, l4_uint16_t>;
  using Total_vfs    = R<0x0E, l4_uint16_t>;
  using Num_vfs      = R<0x10, l4_uint16_t>;
  using Fn_dep       = R<0x12, l4_uint8_t>;
  using Vf_offset    = R<0x14, l4_uint16_t>;
  using Vf_stride    = R<0x16, l4_uint16_t>;
  using Vf_device_id = R<0x1A, l4_uint16_t>;
  using Supported_ps = R<0x1C, l4_uint32_t>;
  using System_ps    = R<0x20, l4_uint32_t>;
  using Vf_bar0      = R<0x24, l4_uint32_t>;
  using Vf_bar5      = R<0x38, l4_uint32_t>;
  using Vf_migration_state = R<0x3C, l4_uint32_t>;
};

struct Ari_cap : Capability
{
  enum
  {
    Id   = 0x0e,
    Size = 0x08,
  };

  struct Caps : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, mfvc_func_groups, v);
    CXX_BITFIELD_MEMBER( 1,  1, acs_func_groups, v);
    CXX_BITFIELD_MEMBER( 8, 15, next_func, v);
  };

  struct Ctrl : R<0x06, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0,  0, mfvc_func_groups, v);
    CXX_BITFIELD_MEMBER( 1,  1, acs_func_groups, v);
    CXX_BITFIELD_MEMBER( 4,  6, group, v);
  };
};

struct Acs_cap : Capability
{
  enum
  {
    Id = 0x0d,
  };

  struct Caps : R<0x04, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0, 0, src_validation, v);
    CXX_BITFIELD_MEMBER( 1, 1, translation_blocking, v);
    CXX_BITFIELD_MEMBER( 2, 2, p2p_request_redirect, v);
    CXX_BITFIELD_MEMBER( 3, 3, p2p_completion_redirect, v);
    CXX_BITFIELD_MEMBER( 4, 4, upstream_fwd, v);
    CXX_BITFIELD_MEMBER( 0, 4, f, v);
    CXX_BITFIELD_MEMBER( 5, 5, p2p_egress_ctrl, v);
    CXX_BITFIELD_MEMBER( 6, 6, direct_translated_p2p, v);
    CXX_BITFIELD_MEMBER( 0, 6, features, v);
    CXX_BITFIELD_MEMBER( 8, 15, egress_ctrl_vector_size, v);
  };

  struct Ctrl : R<0x06, l4_uint16_t>
  {
    CXX_BITFIELD_MEMBER( 0, 0, src_validation_enable, v);
    CXX_BITFIELD_MEMBER( 1, 1, translation_blocking_enable, v);
    CXX_BITFIELD_MEMBER( 2, 2, p2p_request_redirect_enable, v);
    CXX_BITFIELD_MEMBER( 3, 3, p2p_completion_redirect_enable, v);
    CXX_BITFIELD_MEMBER( 4, 4, upstream_fwd_enable, v);
    CXX_BITFIELD_MEMBER( 0, 4, f, v);
    CXX_BITFIELD_MEMBER( 5, 5, p2p_egress_ctrl_enable, v);
    CXX_BITFIELD_MEMBER( 6, 6, direct_translated_p2p_enable, v);
    CXX_BITFIELD_MEMBER( 0, 6, enabled, v);
  };
};

struct Resizable_bar_cap : Capability
{
  enum { Id = 0x15 };

  struct Bar_cap
  {
    l4_uint32_t v; /**< Value container for the register */

    CXX_BITFIELD_MEMBER( 4,  4, sup_1mb,   v);
    CXX_BITFIELD_MEMBER( 5,  5, sup_2mb,   v);
    CXX_BITFIELD_MEMBER( 6,  6, sup_4mb,   v);
    CXX_BITFIELD_MEMBER( 7,  7, sup_8mb,   v);
    CXX_BITFIELD_MEMBER( 8,  8, sup_16mb,  v);
    CXX_BITFIELD_MEMBER( 9,  9, sup_32mb,  v);
    CXX_BITFIELD_MEMBER(10, 10, sup_64mb,  v);
    CXX_BITFIELD_MEMBER(11, 11, sup_128mb, v);
    CXX_BITFIELD_MEMBER(12, 12, sup_256mb, v);
    CXX_BITFIELD_MEMBER(13, 13, sup_512mb, v);
    CXX_BITFIELD_MEMBER(14, 14, sup_1gb,   v);
    CXX_BITFIELD_MEMBER(15, 15, sup_2gb,   v);
    CXX_BITFIELD_MEMBER(16, 16, sup_4gb,   v);
    CXX_BITFIELD_MEMBER(17, 17, sup_8gb,   v);
    CXX_BITFIELD_MEMBER(18, 18, sup_16gb,  v);
    CXX_BITFIELD_MEMBER(19, 19, sup_32gb,  v);
    CXX_BITFIELD_MEMBER(20, 20, sup_64gb,  v);
    CXX_BITFIELD_MEMBER(21, 21, sup_128gb, v);
    CXX_BITFIELD_MEMBER(22, 22, sup_256gb, v);
    CXX_BITFIELD_MEMBER(23, 23, sup_512gb, v);
    CXX_BITFIELD_MEMBER(24, 24, sup_1tb,   v);
    CXX_BITFIELD_MEMBER(25, 25, sup_2tb,   v);
    CXX_BITFIELD_MEMBER(26, 26, sup_4tb,   v);
    CXX_BITFIELD_MEMBER(27, 27, sup_8tb,   v);
    CXX_BITFIELD_MEMBER(28, 28, sup_16tb,  v);
    CXX_BITFIELD_MEMBER(29, 29, sup_32tb,  v);
    CXX_BITFIELD_MEMBER(30, 30, sup_64tb,  v);
    CXX_BITFIELD_MEMBER(31, 31, sup_128tb, v);
  };

  struct Bar_ctrl
  {
    l4_uint32_t v; /**< Value container for the register */

    CXX_BITFIELD_MEMBER( 0,  2, index,     v);
    CXX_BITFIELD_MEMBER( 5,  7, num_bars,  v);
    CXX_BITFIELD_MEMBER( 8, 13, size,      v);
    CXX_BITFIELD_MEMBER(16, 16, sup_256tb, v);
    CXX_BITFIELD_MEMBER(17, 17, sup_512tb, v);
    CXX_BITFIELD_MEMBER(18, 18, sup_1pb,   v);
    CXX_BITFIELD_MEMBER(19, 19, sup_2pb,   v);
    CXX_BITFIELD_MEMBER(20, 20, sup_4pb,   v);
    CXX_BITFIELD_MEMBER(21, 21, sup_8pb,   v);
    CXX_BITFIELD_MEMBER(22, 22, sup_16pb,  v);
    CXX_BITFIELD_MEMBER(23, 23, sup_32pb,  v);
    CXX_BITFIELD_MEMBER(24, 24, sup_64pb,  v);
    CXX_BITFIELD_MEMBER(25, 25, sup_128pb, v);
    CXX_BITFIELD_MEMBER(26, 26, sup_256pb, v);
    CXX_BITFIELD_MEMBER(27, 27, sup_512pb, v);
    CXX_BITFIELD_MEMBER(28, 28, sup_1eb,   v);
    CXX_BITFIELD_MEMBER(29, 29, sup_2eb,   v);
    CXX_BITFIELD_MEMBER(30, 30, sup_4eb,   v);
    CXX_BITFIELD_MEMBER(31, 31, sup_8eb,   v);
  };

  // BAR control register 0 is always present
  struct Bar_ctrl_0 : Bar_ctrl
  {
    enum { Ofs = 8 /**< Offset of the register in bytes */ };
  };
};

} }

