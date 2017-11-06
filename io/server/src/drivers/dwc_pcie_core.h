/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2017, 2022-2023 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *            Frank Mehnert <frank.mehnert@kernkonzept.com>
 */
#pragma once

#include <l4/cxx/type_traits>

#include <l4/drivers/hw_mmio_register_block>
#include "hw_device.h"
#include <pci-root.h>

/**
 * Designware PCIe core driver
 */
class Dwc_pcie :
  public Hw::Device,
  public Hw::Pci::Root_bridge
{
public:
  template<typename ...ARGS>
  Dwc_pcie(ARGS && ...args)
  : Hw::Pci::Root_bridge(cxx::forward<ARGS>(args)...),
    _regs_base(~0), _regs_size(~0), _cfg_base(~0), _cfg_size(~0),
    _mem_base(~0), _mem_size(~0), _cpu_fixup(~0), _num_lanes(1)
  {
    // the set of mandatory properties
    register_property("cfg_base", &_cfg_base);
    register_property("cfg_size", &_cfg_size);
    register_property("regs_base", &_regs_base);
    register_property("regs_size", &_regs_size);
    register_property("mem_base", &_mem_base);
    register_property("mem_size", &_mem_size);
    register_property("cpu_fixup", &_cpu_fixup);

    // the set of optional properties, if not set defaults will be applied
    register_property("lanes", &_num_lanes);
  }

  /**
   * Map and initialize all required resources.
   *
   * \retval  L4_EOK     Success.
   * \retval -L4_EINVAL  One of the mandatory resources could not be
   *                     initialized.
   * \retval -L4_ENOMEM  One of the resources could not be mapped.
   */
  int host_init();

  /**
   * Definition of the port logic register offsets.
   *
   * The port logic registers are vendor-specific and are mainly used to
   * configure the PCIe core. The registers reside in the application register
   * section of the configuration space starting at 0x700.
   */
  enum Port_logic
  {
    Port_logic          = 0x700,
    Link_ctrl           = Port_logic +  0x10,
    Debug0              = Port_logic +  0x28,
    Debug1              = Port_logic +  0x2c,
    Gen2                = Port_logic + 0x10c, // PCIE_LINK_WIDTH_SPEED_CONTROL
    Msi_ctrl_lower_addr = Port_logic + 0x120,
    Msi_ctrl_upper_addr = Port_logic + 0x124,
    Misc_control_1      = Port_logic + 0x1bc,
    Iatu_viewport       = Port_logic + 0x200,
    Iatu_ctrl_1         = Port_logic + 0x204,
    Iatu_ctrl_2         = Port_logic + 0x208,
    Iatu_lower_base     = Port_logic + 0x20c,
    Iatu_upper_base     = Port_logic + 0x210,
    Iatu_limit          = Port_logic + 0x214,
    Iatu_lower_target   = Port_logic + 0x218,
    Iatu_upper_target   = Port_logic + 0x21c
  };

  /**
   * Bit for the Link_ctrl register
   */
  enum Pl_link_ctrl
  {
    Mode_enable_shift = 16,
    Mode_enable_mask  = 0x3f << Mode_enable_shift,
  };

  /**
   * Constants for the Link Mode Enable bits in the Pl_link_ctrl register.
   */
  enum Link_mode
  {
    Link_1_lanes  = 0x1,
    Link_2_lanes  = 0x3,
    Link_4_lanes  = 0x7,
    Link_8_lanes  = 0xf,
    Link_16_lanes = 0x1f,
  };

  /**
   * Bits for the Gen2 control register
   */
  enum Pl_gen2
  {
    /**
     * Indicate the number of lanes to check and to ignore broken lanes.
     */
    Lane_enable_shift = 8,

    /**
     * Mask for the number of lanes.
     *
     * DesignWare documents bits 8-16. i.MX8 documents bits 8-12. The Linux
     * driver uses bits 8-12 in PORT_LOGIC_LINK_WIDTH_MASK. As bits 13-16 have
     * other meanings on i.MX8 we use bits 8-12 to not break i.MX8.
     */
    Lane_enable_mask  = 0x1f << Lane_enable_shift,
    /**
     * Indicates to LTSSM whether to initiate a speed change.
     */
    Speed_change_shift = 17,
  };

  /**
   * The total number of available iATU regions (=viewports) is configuration
   * dependent. A driver must not set a viewport greater than
   * CX_ATU_NUM_[OUT,IN]BOUND_REGIONS-1. However, the default configuration
   * of the core seems to always have at least 2 outbound viewports (see the
   * Linux driver or e.g. the iMX6q TRM.
   */
  enum Iatu_vp
  {
    Idx0 = 0,  ///< Viewport 1 index
    Idx1 = 1,  ///< Viewport 2 index
    Idx2 = 2,  ///< Viewport 3 index
    Idx3 = 3,  ///< Viewport 4 index
    Dir_mask = 1 << 31,
    Outbound = 0 << 31,
    Inbound  = 1 << 31,
  };

  enum Iatu_ctrl_1_values
  {
    Type_mask = 0x1f,
  };

  enum Iatu_ctrl_2_values
  {
    Region_enable = 1UL << 31, ///< Enable address translation for the configured region
  };

  /**
   * Definition of PCIe Transaction Layer Protocol (TLP) types
   */
  enum Tlp_type
  {
    Mem  = 0, ///< Memory read and write requests
    Io   = 2, ///< I/O read and write requests
    Cfg0 = 4, ///< Config type 0 read and write requests
    Cfg1 = 5, ///< Config type 1 read and write requests
  };

  /**
   * Program the internal address translation unit (iATU).
   *
   * \param index        The viewport index that should be programmed. See
   *                     #Iatu_vp.
   * \param base_addr    The start address of the region that should be
   *                     translated.
   * \param size         The size of the region that should be translated. It
   *                     depends on the platform how many bits are considered.
   *                     For instance, for i.MX8, only bits 0..11 are relevant.
   * \param target_addr  The start address of the translated address.
   * \param tlp_type     The access type of the transaction layer protocol. See
   *                     #Tlp_type.
   * \param dir          Determine whether the region is an inbound or outbound
   *                     region. This parameter is optional and defaults to
   *                     #Iatu_vp::Outbound.
   *
   * The iATU registers are programmed through an index (viewport) which needs
   * to be written into the Iatu_viewport register before accessing the other
   * iATU registers.
   */
  void set_iatu_region(unsigned index, l4_uint64_t base_addr, l4_uint32_t size,
                       l4_uint64_t target_addr, unsigned tlp_type,
                       unsigned dir = Outbound);

  typedef Hw::Pci::Cfg_addr Cfg_addr;
  typedef Hw::Pci::Cfg_width Cfg_width;

  /**
   * Configure and return a PCI config space register set.
   *
   * \param addr  The address of the config space that should be setup.
   *
   * \return  The config space register set for the given config address.
   */
  L4drivers::Register_block<32> cfg_regs(Cfg_addr addr);

  /**
   * Perform a PCI config space read.
   *
   * \param      addr   The config space address to read from.
   * \param[out] value  This parameter returns the value read from addr.
   * \param      width  The width of the config space access. See #Cfg_width.
   *
   * \retval 0     The access was completed successfully.
   * \retval -EIO  Invalid access width.
   */
  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;

  /**
   * Perform a PCI config space write.
   *
   * \param addr   The config space address to write to.
   * \param value  The value to write to addr.
   * \param width  The width of the config space access. See #Cfg_width.
   *
   * \retval 0     The access was completed successfully.
   * \retval -EIO  Invalid access width.
   */
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

  /**
   * Returns whether the PCIe link is running.
   *
   * \retval true   PCIe link is running.
   * \retval false  PCIe link is not running.
   *
   * This function checks the Debug Register 1 from the vendor-specific
   * registers in the PCIe extended configuration space. Bit 4 in this register
   * reports whether LTSSM was able to bring the PHY link up. Bit 29 signals
   * that LTSSM is still performing link training.
   *
   * This function can be overridden by a sub class.
   */
  virtual bool link_up()
  {
    l4_uint32_t val = _regs[Port_logic::Debug1];
    return ((val & (1 << 4)) && !(val & 1 << 29));
  }

  /**
   * Setup the PCIe core as root complex (RC)
   */
  void setup_rc();

protected:
  L4drivers::Register_block<32> _regs; ///< The PCIe IP core registers
  L4drivers::Register_block<32> _cfg;  ///< The PCI config space region

  Int_property _regs_base;  ///< Base address of the PCIe core registers
  Int_property _regs_size;  ///< Size of the PCIe core register space
  Int_property _cfg_base;   ///< Base address of the PCI config space region
  Int_property _cfg_size;   ///< Size of the PCI config space region
  Int_property _mem_base;   ///< Base address of the memory region
  Int_property _mem_size;   ///< Size of the memory region
  Int_property _cpu_fixup;  ///< CPU fixup for accessing config space (optional)
  Int_property _num_lanes;  ///< Number of PCIe lanes

private:
  /**
   * Check whether the config space address belongs to a valid device
   *
   * \param addr  Config space address of the device.
   *
   * \retval true   The device is valid.
   * \retval false  The device is invalid and cannot be accessed.
   */
  bool device_valid(Cfg_addr addr);
};
