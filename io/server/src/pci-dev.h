/*
 * Copyright (C) 2010-2021 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <pci-if.h>
#include <pci-cfg.h>
#include <pci-saved-config.h>

#include <l4/cxx/hlist>
#include <cassert>

namespace Hw { namespace Pci {

/**
 * Cache for often needed PCI config space infos.
 *
 * This cache is created and filled during device scan, and
 * in particular, before the device node is allocated.
 * This helps to use the PCI config space of a device node
 * to decide which kind of object needs to be allocated.
 *
 * Usually the config cache is then passed to the new Pci::Dev node
 * during construction and stored in the device node.
 */
class Config_cache : public Config
{
public:
  l4_uint32_t vendor_device = 0;
  l4_uint32_t cls_rev = 0;
  l4_uint32_t subsys_ids = 0;
  l4_uint8_t  hdr_type = 0;
  l4_uint8_t  irq_pin = 0;
  l4_uint8_t  cap_list = 0; ///< offset of the capability pointer register

  l4_uint8_t  pm_cap = 0; ///< offset of power managment cap
  l4_uint8_t  pcie_cap = 0; ///< offset of PCIe cap
  l4_uint8_t  pcie_type = 0; ///< type from PCIe cap if available

  Config_cache() = default;
  explicit Config_cache(Config const &cfg) : Config(cfg) {}

  int vendor() const { return vendor_device & 0xffff; }
  int device() const { return (vendor_device >> 16) & 0xffff; }
  bool is_multi_function() const { return hdr_type & 0x80; }

  int type() const { return hdr_type & 0x7f; }
  int nbars() const
  {
    switch (type())
      {
      case 0: return 6;
      case 1: return 2;
      case 2: return 1;
      default: return 0;
      }
  }

  l4_uint8_t base_class() const { return cls_rev >> 24; }
  l4_uint8_t sub_class() const { return (cls_rev >> 16) & 0xff; }
  l4_uint8_t interface() const { return (cls_rev >> 8) & 0xff; }
  l4_uint8_t rev_id() const { return cls_rev & 0xff; }

  void fill(l4_uint32_t vendor_device, Config const &c);

private:
  void _discover_pci_caps(Config const &c);
};

class Extended_cap_handler;

class Dev :
  public virtual If,
  private Io_irq_pin::Msi_src,
  private Dma_requester
{
public:
  class Flags
  {
    l4_uint16_t _raw;

  public:
    CXX_BITFIELD_MEMBER(0, 0, discovered, _raw);
    CXX_BITFIELD_MEMBER(1, 1, msi, _raw);
    CXX_BITFIELD_MEMBER(2, 2, state_saved, _raw);

    Flags() : _raw(0) {}
  };

  /**
   * Intel VT-d interrupt remapping table entry format.
   *
   * This format is used by Fiasco as `source` parameter to the system Icu
   * `msi_info()` call on x86. It it programmed directly into the IOMMU.
   */
  struct Vtd_irte_src_id
  {
    l4_uint64_t v = 0;
    Vtd_irte_src_id(l4_uint64_t v) : v(v) {}

    CXX_BITFIELD_MEMBER(18, 19, svt, v);

    enum Source_validation_type
    {
      Svt_none         = 0,
      Svt_requester_id = 1,
      Svt_bus_range    = 2,
    };

    CXX_BITFIELD_MEMBER(16, 17, sq, v);

    // svt == Svt_requester_id
    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);

    // svt == Svt_bus_range
    CXX_BITFIELD_MEMBER( 8, 15, start_bus, v);
    CXX_BITFIELD_MEMBER( 0,  7, end_bus, v);
  };

  /**
   * Intel VT-d dma source id.
   *
   * This format is used by Fiasco as `src_id` parameter to Iommu::bind() on
   * x86.
   */
  struct Vtd_dma_src_id
  {
    l4_uint64_t v = 0;
    Vtd_dma_src_id(l4_uint64_t v) : v(v) {}

    CXX_BITFIELD_MEMBER(18, 19, match, v);

    enum Mode
    {
      Match_requester_id = 1,
      Match_bus          = 2,
    };

    //CXX_BITFIELD_MEMBER( 0, 15, sid, v);
    CXX_BITFIELD_MEMBER(16, 17, phantomfn, v);

    // match == Match_requester_id
    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);

    // match == Match_bus
    CXX_BITFIELD_MEMBER( 8, 15, whole_bus, v);
  };

  Msi_src *get_msi_src() override
  {
    if (_external_msi_src)
      return _external_msi_src;

    return this;
  }

  l4_uint64_t get_msi_src_id(Msi_mgr *mgr) override
  {
    if (mgr)
      _msi_mgrs.add(mgr);

    Vtd_irte_src_id id(0);
    id.svt() = Vtd_irte_src_id::Svt_requester_id;
    id.sq() = _phantomfn_bits;
    id.bus() = bus_nr();
    id.devfn() = devfn();
    return id.v;
  }

  ::Dma_requester *get_dma_src() override
  {
    if (_external_dma_src)
      return _external_dma_src;

    return this;
  }

  l4_uint64_t get_dma_src_id() override
  {
    Vtd_dma_src_id id(0);
    id.match() = Vtd_dma_src_id::Match_requester_id;
    id.phantomfn() = _phantomfn_bits;
    id.bus() = bus_nr();
    id.devfn() = devfn();
    return id.v;
  }

  /// Get the Dma_requester for devices downstream if this is a bridge
  virtual ::Dma_requester *get_downstream_dma_src()
  {
    return get_dma_src();
  }

  void add_saved_cap(Saved_cap *cap) { _saved_state.add_cap(cap); }

  void enable_bus_master() override
  {
    auto c = config();
    l4_uint16_t v = c.read<l4_uint16_t>(Config::Command);

    // bus mastering is off
    if (!(v & 0x4))
      c.write<l4_uint16_t>(Config::Command, (v | 0x4));
  }

protected:
  cxx::H_list_t<Msi_mgr> _msi_mgrs;

  Hw::Device *_host;
  Bridge_if *_bridge = nullptr;
  Io_irq_pin::Msi_src *_external_msi_src = nullptr;
  ::Dma_requester *_external_dma_src = nullptr;

public:
  Config_cache const cfg;
  Flags flags;

private:
  unsigned char _phantomfn_bits = 0;
  Resource *_bars[6];
  Resource *_rom;

  Transparent_msi *_transp_msi = 0;

  Saved_config _saved_state;

  void parse_msi_cap(Cfg_addr cap_ptr);

public:
  enum Cfg_status
  {
    CS_cap_list = 0x10, // ro
    CS_66_mhz   = 0x20, // ro
    CS_fast_back2back_cap        = 0x00f0,
    CS_master_data_paritey_error = 0x0100,
    CS_devsel_timing_fast        = 0x0000,
    CS_devsel_timing_medium      = 0x0200,
    CS_devsel_timing_slow        = 0x0400,
    CS_sig_target_abort          = 0x0800,
    CS_rec_target_abort          = 0x1000,
    CS_rec_master_abort          = 0x2000,
    CS_sig_system_error          = 0x4000,
    CS_detected_parity_error     = 0x8000,
  };

  enum Cfg_command
  {
    CC_io          = 0x0001,
    CC_mem         = 0x0002,
    CC_bus_master  = 0x0004,
    CC_serr        = 0x0100,
    CC_int_disable = 0x0400,
  };

  Resource *bar(int b) const override
  {
    return _bars[b];
  }

  void set_bar(unsigned bar, Resource *r)
  { _bars[bar] = r; }

  Resource *rom() const override
  { return _rom; }

  l4_uint32_t recheck_bars(unsigned decoders) override;
  l4_uint32_t checked_cmd_read() override;
  l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) override;
  bool enable_rom() override;

  explicit Dev(Hw::Device *host, Bridge_if *bridge,
               Msi_src *ext_msi, ::Dma_requester *ext_dma,
               Config_cache const &cfg)
  : _host(host), _bridge(bridge),
    _external_msi_src(ext_msi), _external_dma_src(ext_dma), cfg(cfg),
    _rom(0)
  {
    for (unsigned i = 0; i < sizeof(_bars)/sizeof(_bars[0]); ++i)
      _bars[i] = 0;
  }

  Bridge_if *bridge() const { return _bridge; }

  Hw::Device *host() const { return _host; }

  bool supports_msi() const override
  { return flags.msi(); }

  Cfg_addr cfg_addr(unsigned reg = 0) const
  { return cfg.addr() + reg; }

  Config config(unsigned reg = 0) const
  { return cfg + reg; }

  Cap find_pci_cap(unsigned char id);
  Extended_cap find_ext_cap(unsigned id);

  bool is_pcie() const
  { return cfg.pcie_cap != 0; }

  Cap pcie_cap()
  { return cfg.pcie_cap ? Cap(config(cfg.pcie_cap)) : Cap(); }

  using If::cfg_read;
  using If::cfg_write;
  int cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width w) override
  {
    return cfg.cfg_spc()->cfg_read(cfg_addr(reg), value, w);
  }

  int cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width w) override
  {
    return cfg.cfg_spc()->cfg_write(cfg_addr(reg), value, w);
  }

  l4_uint32_t vendor_device_ids() const override;
  l4_uint32_t class_rev() const override;
  l4_uint32_t subsys_vendor_ids() const override;
  unsigned bus_nr() const override;
  Config_space *config_space() const override
  { return cfg.cfg_spc(); }

  void setup(Hw::Device *host) override;
  virtual void discover_resources(Hw::Device *host);
  virtual void discover_bus(Hw::Device *) {}

  bool match_cid(cxx::String const &cid) const override;
  void dump(int indent) const override;

  unsigned devfn() const override
  { return cfg.addr().devfn(); }

  unsigned phantomfn_bits() const override
  { return _phantomfn_bits; }

  void set_phantomfn_bits(unsigned char bits)
  { _phantomfn_bits = bits & 3; }

  unsigned disable_decoders();
  void restore_decoders(l4_uint16_t cmd);

  bool check_pme_status();

  void pm_save_state(Hw::Device *) override;
  void pm_restore_state(Hw::Device *) override;

  static void add_ext_cap_handler(Extended_cap_handler *h)
  { _ext_cap_handlers.add(h); }

  unsigned segment_nr() const override
  { return bridge()->segment(); }

private:
  int discover_bar(int bar);
  void discover_expansion_rom();
  void discover_pci_caps();
  void discover_pcie_caps();

  static cxx::H_list_t<Extended_cap_handler> _ext_cap_handlers;
};

class Extended_cap_handler : public cxx::H_list_item_t<Extended_cap_handler>
{
public:
  virtual bool handle_cap(Dev *dev, Extended_cap cap) const = 0;
  virtual bool match(l4_uint32_t hdr) const = 0;

protected:
  Extended_cap_handler() = default;
  ~Extended_cap_handler() = default;
};

template<unsigned CAP_ID>
class Extended_cap_handler_t : public Extended_cap_handler
{
public:
  bool match(l4_uint32_t hdr) const override
  {
    return (hdr & 0xffff) == CAP_ID;
  }

  Extended_cap_handler_t() { Dev::add_ext_cap_handler(this); }
};

} }
