/*
 * Copyright (C) 2010-2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <pci-cfg.h>
#include <hw_device.h>
#include <irqs.h>

namespace Hw { namespace Pci {

/**
 * Abstract interface of a generic PCI device
 */
class If : public virtual Dev_feature
{
public:
  virtual Resource *bar(int) const = 0;
  virtual Resource *rom() const = 0;
  virtual bool supports_msi() const = 0;

  virtual int cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width) = 0;
  virtual int cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width) = 0;

  virtual l4_uint32_t vendor_device_ids() const = 0;
  virtual l4_uint32_t class_rev() const = 0;
  virtual l4_uint32_t subsys_vendor_ids() const = 0;
  virtual l4_uint32_t recheck_bars(unsigned decoders) = 0;
  virtual l4_uint32_t checked_cmd_read() = 0;
  virtual l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) = 0;
  virtual bool enable_rom() = 0;

  virtual void enable_bus_master() = 0;

  virtual unsigned segment_nr() const = 0;
  virtual unsigned bus_nr() const = 0;
  virtual unsigned devfn() const = 0;
  unsigned device_nr() const { return devfn() >> 3; }
  unsigned function_nr() const { return devfn() & 7; }
  virtual unsigned phantomfn_bits() const = 0;
  virtual Config_space *config_space() const = 0;
  virtual Io_irq_pin::Msi_src *get_msi_src() = 0;
  virtual Dma_requester *get_dma_src() = 0;

  virtual ~If() = 0;

  Cfg_addr cfg_addr(unsigned reg = 0) const
  { return Cfg_addr(bus_nr(), device_nr(), function_nr(), reg); }

  Config config(unsigned reg = 0) const
  { return Config(cfg_addr(reg), config_space()); }
};

inline If::~If() = default;

/**
 * DMA requester ID of a PCI device.
 *
 * Some bridges may take the ownership of some or all memory transactions of a
 * device. This applies to MSI writes as well. So we not only store the ID
 * itself but also the nature of their origin so that platform code can take
 * the appropriate action.
 */
struct Dma_requester_id
{
  enum class Type : unsigned char
  {
    /**
     * Invalid DMA requester ID.
     */
    None,

    /**
     * Alias DMA requester ID.
     *
     * PCIe-to-PCI(-X) bridges may alias transactions of an originating
     * device on the bus. The upstream bridges and the IOMMU must thus be
     * prepared to see different device or function numbers for the same
     * device.
     */
    Alias,

    /**
     * DMA requester ID rewrite.
     *
     * Legacy PCI bridges take the ownership of all downstream devices.
     * Effectively, all downstream IDs are rewritten to the bridge requester
     * ID.
     */
    Rewrite
  };

  l4_uint32_t addr = 0;
  Type type = Type::None;

  CXX_BITFIELD_MEMBER(16, 31, segment,  addr);
  CXX_BITFIELD_MEMBER( 8, 15, bus,      addr);
  CXX_BITFIELD_MEMBER( 3,  7, dev,      addr);
  CXX_BITFIELD_MEMBER( 0,  2, fn,       addr);
  CXX_BITFIELD_MEMBER( 0,  7, devfn,    addr);

  constexpr Dma_requester_id() = default;
  constexpr Dma_requester_id(Type t, unsigned segment, unsigned bus,
                             unsigned devfn)
  : addr(segment << 16 | bus << 8 | devfn), type(t)
  {}

  static constexpr Dma_requester_id alias(unsigned segment, unsigned bus,
                                          unsigned devfn)
  { return Dma_requester_id(Type::Alias, segment, bus, devfn); }

  static constexpr Dma_requester_id rewrite(unsigned segment, unsigned bus,
                                            unsigned devfn)
  { return Dma_requester_id(Type::Rewrite, segment, bus, devfn); }

  operator bool() const { return type != Type::None; }
  bool is_alias() const { return type == Type::Alias; }
  bool is_rewrite() const { return type == Type::Rewrite; }

  char const *as_cstr() const
  {
    switch (type)
      {
        case Type::None:    return "none";
        case Type::Alias:   return "alias";
        case Type::Rewrite: return "rewrite";
        default:            return "?";
      }
  }
};

class Bridge_if
{
protected:
  ~Bridge_if() = default;

public:
  virtual unsigned alloc_bus_number() = 0;
  virtual bool check_bus_number(unsigned bus) = 0;
  virtual bool ari_forwarding_enable() = 0;
  virtual unsigned segment() const = 0;
  virtual Dma_requester_id dma_alias() const = 0;
};

class Transparent_msi
{
public:
  virtual l4_uint32_t filter_cmd_read(l4_uint32_t cmd) = 0;
  virtual l4_uint16_t filter_cmd_write(l4_uint16_t cmd, l4_uint16_t ocmd) = 0;
  virtual ~Transparent_msi() = default;
};


} }

