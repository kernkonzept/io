/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

/**
 * \file
 * Driver for the PCIe controller on i.MX8 MQ boards.
 *
 * The Linux device tree for such devices looks like this:
 * \code{.dts}
 * pciea: pcie@0x5f000000 {
 *        compatible = "fsl,imx8qm-pcie","snps,dw-pcie";
 *        reg = <0x5f000000 0x10000>,
 *              <0x6ff00000 0x80000>,
 *              <0x5f080000 0xf0000>;
 *        reg-names = "dbi", "config", "hsio";
 *        #address-cells = <3>;
 *        #size-cells = <2>;
 *        device_type = "pci";
 *        bus-range = <0x00 0xff>;
 *        ranges = <0x81000000 0 0x00000000 0x6ff80000 0 0x00010000
 *                  0x82000000 0 0x60000000 0x60000000 0 0x0ff00000>;
 *        num-lanes = <1>;
 *        num-viewport = <4>;
 *        interrupts = <GIC_SPI 70 IRQ_TYPE_LEVEL_HIGH>,
 *                     <GIC_SPI 72 IRQ_TYPE_LEVEL_HIGH>;
 *        interrupt-names = "msi", "dma";
 *         #interrupt-cells = <1>;
 *        interrupt-map-mask = <0 0 0 0x7>;
 *        interrupt-map =  <0 0 0 1 &gic 0 73 4>,
 *                         <0 0 0 2 &gic 0 74 4>,
 *                         <0 0 0 3 &gic 0 75 4>,
 *                         <0 0 0 4 &gic 0 76 4>;
 *        clocks = <&pciea_lpcg 0>,
 *                 <&pciea_lpcg 1>,
 *                 <&pciea_lpcg 2>,
 *                 <&phyx2_lpcg 0>,
 *                 <&phyx2_crr0_lpcg 0>,
 *                 <&pciea_crr2_lpcg 0>,
 *                 <&misc_crr5_lpcg 0>;
 *        clock-names = "pcie", "pcie_bus", "pcie_inbound_axi",
 *                      "pcie_phy", "phy_per", "pcie_per", "misc_per";
 *        power-domains = <&pd IMX_SC_R_PCIE_A>,
 *                        <&pd IMX_SC_R_SERDES_0>,
 *                        <&pd IMX_SC_R_HSIO_GPIO>;
 *        power-domain-names = "pcie", "pcie_phy", "hsio_gpio";
 *        fsl,max-link-speed = <3>;
 *        hsio-cfg = <PCIEAX1PCIEBX1SATA>;
 *        local-addr = <0x40000000>;
 *        ext_osc = <1>;
 *        pinctrl-0 = <&pinctrl_pciea>;
 *        reset-gpio = <&lsio_gpio3 14 GPIO_ACTIVE_LOW>;
 *        ...
 * };
 * ...
 * pinctrl_pciea: pcieagrp {
 *        fsl,pins = <IMX8QM_SAI1_RXFS_LSIO_GPIO3_IO14 0x00000021>;
 * };
 * ...
 * hsio_refa_clk: clock-hsio-refa {
 *        ...
 *        enable-gpios = <&lsio_gpio4 27 GPIO_ACTIVE_LOW>;
 * };
 * ...
 * hsio_refb_clk: clock-hsio-refb {
 *        ...
 *        enable-gpios = <&lsio_gpio4 1 GPIO_ACTIVE_LOW>;
 * };
 * ...
 * pciea_lpcg: clock-controller@5f050000 {
 *        compatible = "fsl,imx8qxp-lpcg";
 *        reg = <0x5f050000 0x10000>;
 *        #clock-cells = <1>;
 *        clocks = <&hsio_axi_clk>, <&hsio_axi_clk>, <&hsio_axi_clk>;
 *        bit-offset = <16 20 24>;
 *        clock-output-names = "hsio_pciea_mstr_axi_clk",
 *                             "hsio_pciea_slv_axi_clk",
 *                             "hsio_pciea_dbi_axi_clk";
 *        power-domains = <&pd IMX_SC_R_PCIE_A>;
 * };
 * ...
 * phyx2_crr0_lpcg: clock-controller@5f0a0000 {
 *        compatible = "fsl,imx8qxp-lpcg";
 *        reg = <0x5f0a0000 0x10000>;
 *        #clock-cells = <1>;
 *        clocks = <&hsio_per_clk>;
 *        bit-offset = <16>;
 *        clock-output-names = "hsio_phyx2_per_clk";
 *        power-domains = <&pd IMX_SC_R_SERDES_0>;
 * };
 * ...
 * pciea_crr2_lpcg: clock-controller@5f0c0000 {
 *        compatible = "fsl,imx8qxp-lpcg";
 *        reg = <0x5f0c0000 0x10000>;
 *        #clock-cells = <1>;
 *        clocks = <&hsio_per_clk>;
 *        bit-offset = <16>;
 *        clock-output-names = "hsio_pciea_per_clk";
 *        power-domains = <&pd IMX_SC_R_PCIE_A>;
 * };
 * ...
 * misc_crr5_lpcg: clock-controller@5f0f0000 {
 *        compatible = "fsl,imx8qxp-lpcg";
 *        reg = <0x5f0f0000 0x10000>;
 *        #clock-cells = <1>;
 *        clocks = <&hsio_per_clk>;
 *        bit-offset = <16>;
 *        clock-output-names = "hsio_misc_per_clk";
 *        power-domains = <&pd IMX_SC_R_HSIO_GPIO>;
 * };
 * \endcode
 *
 * This entry should be converted into the following io.cfg code. Some of the
 * information of the above device tree is directly implemented in init().
 *
 * \code{.lua}
 * pcie0 = Io.Hw.Pcie_imx8qm(function()
 *   Property.scu     = scu
 *   Property.gpio3   = gpio3   -- see reset-gpio
 *   Property.gpio4   = gpio4   -- see hsio_refa_clk
 *   Property.power   = {
 *                        152,  -- IMX_SC_R_PCIE_A (pciea and HSIO crr)
 *                        153,  -- IMX_SC_R_SERDES_0 (pciea and HSIO crr)
 *                        172,  -- IMX_SC_R_HSIO_GPIO (pciea and HSIO crr)
 *                        169,  -- IMX_SC_R_PCIE_B (pcieb and HSIO crr)
 *                        171,  -- IMX_SC_R_SERDES_1 (pcieb and HSIO crr)
 *                      }
 *   Property.pads    = {
 *                        129, 3, 0x00000021 -- pinctrl_pciea
 *                      }
 *   Property.clks    = {}
 *   Property.regs_base = 0x5f000000 -- 'dbi' regs entry
 *   Property.regs_size = 0x00010000
 *   Property.cfg_base  = 0x6ff00000 -- 'config' regs entry
 *   Property.cfg_size  = 0x00080000
 *   Property.mem_base  = 0x60000000 -- MMIO range from 'ranges'
 *   Property.mem_size  = 0x0ff00000
 *   Property.cpu_fixup = 0x40000000 -- see 'local-addr'
 *   Property.irq_pin_a = 32 + 73,
 *   Property.irq_pin_b = 32 + 74,
 *   Property.irq_pin_c = 32 + 75,
 *   Property.irq_pin_d = 32 + 76,
 *   Property.lanes     = 1
 * end)
 * \endcode
 *
 * Note that the code below only handles the pciea controller properly. The
 * pcieb controller would require adaptions, for example write Hsio_csr_phyx1,
 * Hsio_csr_pcieb and Hsio_lpcg_pcieb.
 */

#include <l4/util/util.h>

#include "dwc_pcie_core.h"
#include "scu_imx8qm.h"
#include "gpio"
#include <l4/drivers/hw_mmio_register_block>

#include <cstdio>
#include <errno.h>
#include <inttypes.h>

#include <l4/sys/kdebug.h>

namespace {

class Pci_irq_router_rs : public Resource_space
{
public:
  bool request(Resource *parent, ::Device *, Resource *child, ::Device *cdev) override;
  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to Pci_irq_router_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Pci_irq_router_rs\n");
    return false;
  }
};

class Pcie_imx8_bridge : public Dwc_pcie
{
  // _regs_base + 0x050000: LPCG_PCIEX2_0
  L4drivers::Register_block<32> _hsio_lpcg_pciea;

  // _regs_base + 0x080000: LPCG_PHYX2_0
  L4drivers::Register_block<32> _hsio_lpcg_phyx2;

  // _regs_base + 0x0a0000: LPCG_PHYX2_CRR0_0
  L4drivers::Register_block<32> _hsio_lpcg_crr_0;

  // _regs_base + 0x0c0000: LPCG_PCIEX2_CRR2_0
  L4drivers::Register_block<32> _hsio_lpcg_crr_2;

  // _regs_base + 0x0f0000: LPCG_MISC_CRR5_0
  L4drivers::Register_block<32> _hsio_lpcg_crr_5;

  // _regs_base + 0x110000: HSIO CRR PHYx2
  L4drivers::Register_block<32> _hsio_csr_phyx2;
  enum Hsio_csr_phyx2_regs
  {
    Phyx2_ctrl0 = 0x0,
    Phyx2_stts0 = 0x4,
  };
  enum Phyx2_ctrl0_values
  {
    Phyx2_ctrl0_apb_rstn_0 = 1U << 0,
    Phyx2_ctrl0_apb_rstn_1 = 1U << 1,
  };
  enum Phyx2_stts0_values
  {
    Phyx2_stts0_lane0_tx_pll_lock = 1U << 4,
  };

  // _regs_base + 0x130000: HSIO CRR for PCIex1
  L4drivers::Register_block<32> _hsio_csr_pciea;
  enum Hsio_csr_pciea_regs
  {
    Pciex1_ctrl0 = 0x0,
    Pciex1_ctrl1 = 0x4,
    Pciex1_ctrl2 = 0x8,
    Pciex1_stts0 = 0xc,
  };
  enum Pciex1_ctrl0_values
  {
    Pciex1_ctrl0_ps_dev_id_shft = 0,
    Pciex1_ctrl0_ps_dev_id_mask = 0xffffU << Pciex1_ctrl0_ps_dev_id_shft,
    Pciex1_ctrl0_ps_rev_id_shft = 16,
    Pciex1_ctrl0_ps_rev_id_mask = 0xffU << Pciex1_ctrl0_ps_rev_id_shft,
    Pciex1_ctrl0_dev_type_shft = 24,
    Pciex1_ctrl0_dev_type_mask = 0xfU << Pciex1_ctrl0_dev_type_shft,
    Pciex1_ctrl0_dev_type_pcie_ep = 0U << Pciex1_ctrl0_dev_type_shft,
    Pciex1_ctrl0_dev_type_lpcie_ep = 1U << Pciex1_ctrl0_dev_type_shft,
    Pciex1_ctrl0_dev_type_root_port = 4U << Pciex1_ctrl0_dev_type_shft,
  };
  enum Pciex1_ctrl2_values
  {
    Pciex1_ctrl2_app_ltssm_enable = 1U << 4,
    Pciex1_ctrl2_button_rst_n = 1U << 21,
    Pciex1_ctrl2_perst_n = 1U << 22,
    Pciex1_ctrl2_power_up_rst_n = 1U << 23,
  };
  enum Pciex1_stts0_values
  {
    Pciex_stts0_rm_req_cor_rst = 1U << 19,
  };

  // _regs_base + 0x160000: HSIO CRR MISC
  L4drivers::Register_block<32> _hsio_csr_misc;
  enum Hsio_csr_misc_regs
  {
    Misc_ctrl0 = 0x0,
    Misc_stts0 = 0x4,
  };
  enum Misc_ctrl0_values
  {
    Misc_ctrl0_iob_rxena = 1U << 0,
    Misc_ctrl0_iob_txena = 1U << 1,
    Misc_ctrl0_phy_x1_epcs_sel = 1U << 12,
    Misc_ctrl0_pcie_ab_select = 1U << 13,
    Misc_ctrl0_clkreqn_out_0 = 1U << 23,
    Misc_ctrl0_clkreqn_out_override = 1U << 25,
  };

  // For resetting PCIe at GPIO.
  Device_property<Hw::Gpio_device> _gpio3;

  Int_property _int_map[4];

  enum Rc_regs
  {
    Pf0_pcie_cap = 0x70,
    Lcr          = Pf0_pcie_cap + 0xc,  ///< Link capability register
  };

  /// PCIe spec 3.0: 7.8.6
  enum Lcr_values
  {
    Pcie_cap_max_link_speed_mask = 0xf,
    Pcie_cap_max_link_speed_gen1 = 1,
  };

  /// See 2.2.2. Each region is 64KiB (0x10000).
  enum
  {
    Hsio_pcie0      = 0x000000,
    Hsio_pcie1      = 0x010000,
    Hsio_sata0      = 0x020000,
    Hsio_lpcg_pciea = 0x050000,
    Hsio_lpcg_pcieb = 0x060000,
    Hsio_lpcg_sata  = 0x070000,
    Hsio_lpcg_phyx2 = 0x080000, // Linux: IMX8QM_PHYX2_LPCG_OFFSET
    Hsio_lpcg_phyx1 = 0x090000,
    Hsio_lpcg_crr_0 = 0x0a0000,
    Hsio_lpcg_crr_1 = 0x0b0000,
    Hsio_lpcg_crr_2 = 0x0c0000,
    Hsio_lpcg_crr_3 = 0x0d0000,
    Hsio_lpcg_crr_4 = 0x0e0000,
    Hsio_lpcg_crr_5 = 0x0f0000,
    Hsio_lpcg_gpio  = 0x100000,
    Hsio_csr_phyx2  = 0x110000, // Linux: IMX8QM_CSR_PHYX2_OFFSET
    Hsio_csr_phyx1  = 0x120000, // Linux: IMX8QM_CSR_PHYX1_OFFSET, for pcieb
    Hsio_csr_pciea  = 0x130000, // Linux: IMX8QM_CSR_PCIEA_OFFSET
    Hsio_csr_pcieb  = 0x140000, // Linux: IMX8QM_CSR_PCIEB_OFFSET
    Hsio_csr_sata   = 0x150000,
    Hsio_csr_misc   = 0x160000, // Linux: IMX8QM_CSR_MISC_OFFSET
    Hsio_gpio       = 0x170000,
    Hsio_phyx2_l0   = 0x180000,
    Hsio_phyx2_l1   = 0x190000,
    Hsio_phyx1      = 0x1a0000,
  };

public:

  explicit Pcie_imx8_bridge(int segment = 0, unsigned bus_nr = 0);
  void init() override;

  bool link_up() override
  { return _regs[Port_logic::Debug1] & (1 << 4); }

  // configuration: PCIEAX1PCIEBX1SATA
  void wait_for_pll_lock()
  {
    for (unsigned retries = 0; retries < 10; ++retries)
      {
        if (_hsio_csr_phyx2[Phyx2_stts0] & Phyx2_stts0_lane0_tx_pll_lock)
          {
            d_printf(DBG_INFO, "%s: PCIe PLL locked\n", name());
            return;
          }
        l4_sleep(2);
      }
    d_printf(DBG_WARN, "%s: warning: PCIe PLL lock timeout!\n", name());
  }

  // check whether link is up and running
  void wait_link_up()
  {
    for (unsigned retries = 0; retries < 10; ++retries)
      {
        if (link_up())
          return;
        l4_usleep(90000);
      }
  }

  void assert_core_reset();
  void deassert_core_reset();

  int int_map(int i) const { return _int_map[i]; }
};

bool
Pci_irq_router_rs::request(Resource *parent, ::Device *pdev,
                           Resource *child, ::Device *cdev)
{
  auto *cd = dynamic_cast<Hw::Device *>(cdev);
  if (!cd)
    return false;

  unsigned pin = child->start();
  if (pin > 3)
    return false;

  auto *pd = dynamic_cast<Pcie_imx8_bridge *>(pdev);
  if (!pd)
    return false;

  unsigned slot = (cd->adr() >> 16);
  unsigned irq_nr = pd->int_map((pin + slot) & 3);

  d_printf(DBG_DEBUG, "%s: Requesting IRQ%c at slot %d => IRQ %d\n",
           cd->get_full_path().c_str(), (int)('A' + pin), slot, irq_nr);

  child->del_flags(Resource::F_relative);
  child->start(irq_nr);
  child->del_flags(Resource::Irq_type_mask);
  child->add_flags(Resource::Irq_type_base | Resource::Irq_type_level_high);

  child->parent(parent);

  return true;
}

Pcie_imx8_bridge::Pcie_imx8_bridge(int segment, unsigned bus_nr)
: Dwc_pcie(segment, bus_nr, this)
{
  set_name_if_empty("pcie_imx8");

  register_property("gpio3", &_gpio3);          // mandatory
  register_property("irq_pin_a", &_int_map[0]); // optional
  register_property("irq_pin_b", &_int_map[1]); // optional
  register_property("irq_pin_c", &_int_map[2]); // optional
  register_property("irq_pin_d", &_int_map[3]); // optional
}

void
Pcie_imx8_bridge::init()
{
  if (Dwc_pcie::host_init())
    return;

  if (_gpio3.dev() == nullptr)
    {
      d_printf(DBG_ERR, "%s: error: 'gpio3' not set.\n", name());
      throw "Pcie_imx8_bridge init error";
    }

  l4_addr_t va = res_map_iomem(_regs_base + Hsio_lpcg_pciea, 0x10000);
  _hsio_lpcg_pciea = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_lpcg_phyx2, 0x10000);
  _hsio_lpcg_phyx2 = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_lpcg_crr_0, 0x10000);
  _hsio_lpcg_crr_0 = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_lpcg_crr_2, 0x10000);
  _hsio_lpcg_crr_2 = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_lpcg_crr_5, 0x10000);
  _hsio_lpcg_crr_5 = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_csr_phyx2, 0x10000);
  _hsio_csr_phyx2 = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_csr_pciea, 0x10000);
  _hsio_csr_pciea = new L4drivers::Mmio_register_block<32>(va);
  va = res_map_iomem(_regs_base + Hsio_csr_misc, 0x10000);
  _hsio_csr_misc = new L4drivers::Mmio_register_block<32>(va);

  assert_core_reset();

  // imx8_pcie_init_phy -- PCIEAX1PCIEBX1SATA
  _hsio_csr_phyx2[Phyx2_ctrl0].set(Phyx2_ctrl0_apb_rstn_0      // APB_RSTN_1=1
                                   | Phyx2_ctrl0_apb_rstn_1);  // APB_RSTN_0=1
  _hsio_csr_misc[Misc_ctrl0].set(Misc_ctrl0_phy_x1_epcs_sel);
  _hsio_csr_misc[Misc_ctrl0].set(Misc_ctrl0_pcie_ab_select);

  // ext_osc=1
  _hsio_csr_misc[Misc_ctrl0].set(Misc_ctrl0_iob_rxena);
  _hsio_csr_misc[Misc_ctrl0].clear(Misc_ctrl0_iob_txena);

  _hsio_csr_pciea[Pciex1_ctrl0].modify(Pciex1_ctrl0_dev_type_mask,
                                       Pciex1_ctrl0_dev_type_root_port);

  deassert_core_reset();

  wait_for_pll_lock();

  setup_rc();

  // Now establish the PCIe link. Start with Gen1 operation mode.

  _regs[Lcr].modify(Pcie_cap_max_link_speed_mask, Pcie_cap_max_link_speed_gen1);

  // enable ltssm: APP_LTSSM_ENABLE=1
  _hsio_csr_pciea[Pciex1_ctrl2].set(Pciex1_ctrl2_app_ltssm_enable);

  wait_link_up();

  // DEFAULT_GEN2_N_FTS=3 (number of fast training sequences during link
  // training)
  _regs[Gen2].modify(0, 3U << 0);

  wait_link_up();
  d_printf(DBG_WARN, "%s: Link %s\n", name(), link_up() ? "up" : "DOWN");

  typedef Hw::Pci::Irq_router_res<Pci_irq_router_rs> Irq_res;
  Irq_res *ir = new Irq_res();
  ir->set_id("IRQR");
  add_resource_rq(ir);

  discover_bus(this, this);
  Hw::Device::init();
}

void
Pcie_imx8_bridge::assert_core_reset()
{
  // We could also do this with using some property mechanism.

  // hsio_phyx2_pclk_0 / bits 0+1
  _hsio_lpcg_phyx2[0x0].modify(3U << 0, 2U << 0);
  l4_sleep(3);
  // hsio_pciea_slv_axi_clk / bits 20+21 (see device tree pciea_lpcg clock 1)
  _hsio_lpcg_pciea[0x0].modify(3U << 20, 2U << 20);
  l4_sleep(3);
  // hsio_pciea_mstr_axi_clk / bits 16+17 (see device tree pciea_lpcg clock 0)
  _hsio_lpcg_pciea[0x0].modify(3U << 16, 2U << 16);
  l4_sleep(3);
  // hsio_pciea_dbi_axi_clk / bits 24+25 (see device tree pciea_lpcg clock 2)
  _hsio_lpcg_pciea[0x0].modify(3U << 24, 2U << 24);
  l4_sleep(3);
  // hsio_pciea_per_clk / bits 16+17 (see device tree pciea_crr2_lpcg clock 0)
  _hsio_lpcg_crr_2[0x0].modify(3U << 16, 2U << 16);
  l4_sleep(3);
  // hsio_phyx2_per_clk / bits 16+17 (see device tree phyx2_crr0_lpcg clock 0)
  _hsio_lpcg_crr_0[0x0].modify(3U << 16, 2U << 16);
  l4_sleep(3);
  // hsio_misc_per_clk / bits 16+17 (see device tree misc_crr5_lpcg clock 0)
  _hsio_lpcg_crr_5[0x0].modify(3U << 16, 2U << 16);
  l4_sleep(3);

  // CLKREQ_0=0
  _hsio_csr_misc[Misc_ctrl0].clear(Misc_ctrl0_clkreqn_out_0);

  // CLKREQ_OVERRIDE_EN_0=1
  _hsio_csr_misc[Misc_ctrl0].set(Misc_ctrl0_clkreqn_out_override);

  // BUTTON_RST_N=1
  _hsio_csr_pciea[Pciex1_ctrl2].set(Pciex1_ctrl2_button_rst_n);

  // PERST_N=1
  _hsio_csr_pciea[Pciex1_ctrl2].set(Pciex1_ctrl2_perst_n);

  // POWER_UP_RST_N=1
  _hsio_csr_pciea[Pciex1_ctrl2].set(Pciex1_ctrl2_power_up_rst_n);
}

void
Pcie_imx8_bridge::deassert_core_reset()
{
  for (unsigned retries = 0; retries < 10; ++retries)
    {
      if (!(_hsio_csr_pciea[Pciex1_stts0] & Pciex_stts0_rm_req_cor_rst))
        break;
      l4_sleep(2);
    }
  if (_hsio_csr_pciea[Pciex1_stts0] & Pciex_stts0_rm_req_cor_rst)
    d_printf(DBG_INFO, "%s: PM_REQ_CORE_RST still set!\n", name());

  wait_for_pll_lock();

  // Reset phy
  Hw::Gpio_chip *gpio3(dynamic_cast<Hw::Gpio_chip*>(_gpio3.dev()));
  gpio3->setup(14, Hw::Gpio_chip::Fix_mode::Output, 0);

  gpio3->set(14, 0); // reset-gpio: 14/GPIO_ACTIVE_LOW assert
  l4_sleep(100);
  gpio3->set(14, 1); // reset-gpio: 14/GPIO_ACTIVE_LOW de-assert
}

// This device requires access to the System Control Unit (SCU). The
// Scu_device::init() function performs the configured SCU operations
// (enabling power, pads, clks) before calling our init() function.
class Pcie_imx8qm : public Hw::Scu_device<Pcie_imx8_bridge>
{};

static Hw::Device_factory_t<Pcie_imx8qm> f("Pcie_imx8qm");

}
