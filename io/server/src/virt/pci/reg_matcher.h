/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <pci-if.h>

/**
 * Matcher for emulating accesses to PCI registers.
 *
 * Assumes and enforces that registers are only accessed naturally aligned with
 * their size, i.e. no partial or overlapping accesses possible.
 *
 * When encountering a non-conforming access the matcher remembers and reports
 * that via the `invalid` function.
 */
struct Reg_matcher
{
  constexpr Reg_matcher(l4_uint32_t offs, Hw::Pci::Cfg_width order)
  : _offs(offs), _size(Hw::Pci::cfg_o_to_size(order))
  {}

  template<typename Reg>
  constexpr bool is_reg()
  {
    if (_invalid || !overlaps_reg<Reg>())
      return false;

    if (!matches_reg<Reg>())
      {
        printf("misaligned or missized reg access @ 0x%x with size %u...\n",
               _offs, _size);
        _invalid = true;
        return false;
      }

    return true;
  }

  constexpr bool in_range(l4_uint32_t from, l4_uint32_t to)
  {
    if (_invalid)
      return false;

    return _offs >= from && _offs + _size < to;
  }

  template<typename Reg_from, typename Reg_to>
  constexpr bool in_range()
  {
    static_assert(l4_uint32_t{Reg_from::Ofs} < l4_uint32_t{Reg_to::Ofs},
                  "Invalid range.");
    return in_range(Reg_from::Ofs, Reg_to::Ofs + sizeof(Reg_to::v));
  }

  /// Incorrectly aligned or sized reg access detected.
  constexpr bool invalid_access() const
  { return _invalid; }

private:
  template<typename Reg>
  constexpr bool matches_reg() const
  { return _offs == Reg::Ofs && _size == sizeof(Reg::v); }

  template<typename Reg>
  constexpr bool overlaps_reg() const
  { return _offs + _size > Reg::Ofs && _offs < Reg::Ofs + sizeof(Reg::v); }

  l4_uint32_t const _offs;
  unsigned const _size;

  bool _invalid = false;
};
