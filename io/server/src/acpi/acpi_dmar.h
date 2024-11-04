/**
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *            Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <acpi.h>
#include <cstdint>

/**
 * Cast an object to a different type with an offset.
 *
 * \tparam T1      Target object type.
 * \tparam T2      Source object type.
 * \param  offset  Byte offset within the source object.
 *
 * \return Target object.
 */
template<typename T1, typename T2> inline
T1 offset_cast(T2 ptr, uintptr_t offset)
{
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr) + offset;
  return reinterpret_cast<T1>(addr);
}

namespace Hw {
namespace Acpi {

struct Dmar_head
{
  Dmar_head const *next() const
  { return offset_cast<Dmar_head const *>(this, length); }

  template<typename T>
  T const *cast() const
  {
    if (type == T::Id)
      return static_cast<T const *>(this);
    return nullptr;
  }

  l4_uint16_t type;
  l4_uint16_t length;
} __attribute__((packed));

template<typename T>
struct Next_iter
{
  Next_iter() : _c(0) {}
  Next_iter(T const *h) : _c(h) {}
  Next_iter const &operator ++ ()
  {
    _c = _c->next();
    return *this;
  }
  T const &operator * () const { return *_c; }
  T const *operator -> () const { return _c; }

  bool operator == (Next_iter const &o) const { return _c == o._c; }
  bool operator != (Next_iter const &o) const { return _c != o._c; }

private:
  T const *_c;
};

struct Dmar_dev_scope
{
  enum Types
    {
      Pci_endpoint = 1,
      Pci_subhierarchy,
      Io_apic,
      Hpet_msi,
      Acpi_namespace_device
    };

  Dmar_dev_scope const *next() const
  { return offset_cast<Dmar_dev_scope const *>(this, length); }

  struct Path_entry
  {
    l4_uint8_t dev;
    l4_uint8_t func;
  };

  Path_entry const *begin() const { return path; }
  Path_entry const *end() const
  { return offset_cast<Path_entry const *>(this, length); }

  l4_uint8_t type;
  l4_uint8_t length;
  l4_uint16_t _rsvd;
  l4_uint8_t enum_id;
  l4_uint8_t start_bus_nr;
  Path_entry path[];
} __attribute__((packed));

struct Dev_scope_vect
{
  typedef Next_iter<Dmar_dev_scope> Iterator;

  Dev_scope_vect() = default;
  Dev_scope_vect(Iterator b, Iterator e)
  : _begin(b), _end(e)
  {}

  Iterator begin() { return _begin; }
  Iterator end() { return _end; }

  Iterator begin() const { return _begin; }
  Iterator end() const { return _end; }

private:
  Iterator _begin, _end;
};

template<typename TYPE>
struct Dmar_dev_scope_mixin
{
  Dev_scope_vect devs() const
  {
    TYPE const *t = static_cast<TYPE const*>(this);
    return Dev_scope_vect(reinterpret_cast<Dmar_dev_scope const *>(t + 1),
                          offset_cast<Dmar_dev_scope const *>(t, t->length));
  }
};

struct Dmar_drhd : Dmar_head, Dmar_dev_scope_mixin<Dmar_drhd>
{
  enum { Id = 0 };
  l4_uint8_t  flags;
  l4_uint8_t  _rsvd;
  l4_uint16_t segment;
  l4_uint64_t register_base;

  CXX_BITFIELD_MEMBER_RO(0, 0, include_pci_all, flags);
} __attribute__((packed));

struct Dmar_rmrr : Dmar_head, Dmar_dev_scope_mixin<Dmar_rmrr>
{
  enum { Id = 1 };
  l4_uint16_t _rsvd;
  l4_uint16_t segment;
  l4_uint64_t base;
  l4_uint64_t limit;
} __attribute__((packed));

struct Dmar_atsr : Dmar_head, Dmar_dev_scope_mixin<Dmar_atsr>
{
  enum { Id = 2 };
  l4_uint8_t  flags;
  l4_uint8_t  _rsvd;
  l4_uint16_t segment;
} __attribute__((packed));

struct Dmar_rhsa : Dmar_head
{
  enum { Id = 3 };

private:
  l4_uint32_t _rsvd;
  l4_uint64_t register_base;
  l4_uint32_t proximity_domain;
} __attribute__((packed));

struct Dmar_andd : Dmar_head
{
  enum { Id = 4 };

private:
  l4_uint8_t _rsvd[3];
  l4_uint8_t acpi_dev_nr;
  l4_uint8_t acpi_name[];
} __attribute__((packed));

}} // namespace Hw::Acpi

/**
 * Header fields common to all ACPI tables.
 */
class Acpi_table_head
{
public:
  char       signature[4];
  l4_uint32_t len;
  l4_uint8_t  rev;
  l4_uint8_t  chk_sum;
  char       oem_id[6];
  char       oem_tid[8];
  l4_uint32_t oem_rev;
  l4_uint32_t creator_id;
  l4_uint32_t creator_rev;

  bool checksum_ok() const
  {
    l4_uint8_t sum = 0;
    for (unsigned i = 0; i < len; ++i)
      sum += *(reinterpret_cast<l4_uint8_t const *>(this) + i);

    return !sum;
  }
} __attribute__((packed));


/**
 * Representation of the ACPI DMAR table.
 */
struct Acpi_dmar : Acpi_table_head
{
  typedef Hw::Acpi::Next_iter<Hw::Acpi::Dmar_head> Iterator;

  Iterator begin() const
  { return reinterpret_cast<Hw::Acpi::Dmar_head const *>(this + 1); }

  Iterator end() const
  { return offset_cast<Hw::Acpi::Dmar_head const *>(this, len); }

  struct Flags
  {
    l4_uint8_t raw;
    CXX_BITFIELD_MEMBER( 0, 0, intr_remap, raw);
    CXX_BITFIELD_MEMBER( 1, 1, x2apic_opt_out, raw);
  };

  l4_uint8_t haw; ///< Host Address Width
  Flags flags;
  l4_uint8_t _rsvd[10];
} __attribute__((packed));
