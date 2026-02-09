/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 * Driver for the PCIe controller on NVIDIA Tegra.
 *
 * _regs_{base,size}: dbi (Data Bus Interface)
 * _appl_{base,size}: appl
 * _atu_{base,size}: atu_dma/atu (Address Translation Unit)
 * _cfg_{base,size}: config (used by map_bus())
 * _mem_{base,size}: MMIO ranges
 *
 * The Linux device tree for such devices looks like this:
 * \code{.dts}
 * pcie@140a0000 {
 *     max-link-speed = 3;
 *     compatible = "nvidia,tegra234-pcie";
 *     power-domains = <&bpmp TEGRA234_POWER_DOMAIN_PCIEX4CA>;
 *     reg = <0x00 0x140a0000 0x0 0x00020000>, // appl registers (128K)
 *           <0x00 0x2a000000 0x0 0x00040000>, // cfg space (256K)
 *           <0x00 0x2a040000 0x0 0x00040000>, // iATU_DMA reg space (256K)
 *           <0x00 0x2a080000 0x0 0x00040000>; // DBI reg space (256K)
 *     reg-names = "appl", "config", "atu_dma", "dbi";
 *     #address-cells = <3>;
 *     #size-cells = <2>;
 *     device_type = "pci";
 *     num-lanes = <4>;
 *     num-viewport = <8>;
 *     linux,pci-domain = <8>;
 *     clocks = <&bpmp TEGRA234_CLK_PEX2_C8_CORE>;
 *     clock-names = "core";
 *     resets = <&bpmp TEGRA234_RESET_PEX2_CORE_8_APB>,
 *              <&bpmp TEGRA234_RESET_PEX2_CORE_8>;
 *     reset-names = "apb", "core";
 *     interrupts = <GIC_SPI 356 IRQ_TYPE_LEVEL_HIGH>, // controller interrupt
 *                  <GIC_SPI 357 IRQ_TYPE_LEVEL_HIGH>; // MSI interrupt
 *     interrupt-names = "intr", "msi";
 *     #interrupt-cells = <1>;
 *     interrupt-map-mask = <0 0 0 0>;
 *     interrupt-map = <0 0 0 0 &gic GIC_SPI 356 IRQ_TYPE_LEVEL_HIGH>;
 *     nvidia,bpmp = <&bpmp 8>;
 *     nvidia,aspm-cmrt-us = <60>;
 *     nvidia,aspm-pwr-on-t-us = <20>;
 *     nvidia,aspm-l0s-entrance-latency-us = <3>;
 *     bus-range = <0x0 0xff>;
 *     ranges =
 *       <0x43000000 0x32 0x40000000 0x32 0x40000000 0x2 0xe8000000>, // 64-bit prf
 *       <0x02000000  0x0 0x40000000 0x35 0x28000000 0x0 0x08000000>, // 32-bit
 *       <0x01000000  0x0 0x2a100000 0x00 0x2a100000 0x0 0x00100000>; // downstream
 *     interconnects = <&mc TEGRA234_MEMORY_CLIENT_PCIE8AR &emc>,
 *                     <&mc TEGRA234_MEMORY_CLIENT_PCIE8AW &emc>;
 *     interconnect-names = "dma-mem", "write";
 *     iommus = <&smmu_niso1 TEGRA234_SID_PCIE8>;
 *     iommu-map = <0x0 &smmu_niso1 TEGRA234_SID_PCIE8 0x1000>;
 *     iommu-map-mask = <0x0>;
 *     dma-coherent;
 *     ...
 * };
 * \endcode
 *
 * This entry should be converted into the following io.cfg code. Some of the
 * information of the above device tree is directly implemented in init().
 *
 * \code{.lua}
 * pcie0 = Hw.Pcie_tegra194(function()
 *   Property.appl_base    =   0x140a0000 -- 'appl'
 *   Property.appl_size    =      0x20000
 *   Property.cfg_base     =   0x2a000000 -- 'config'
 *   Property.cfg_size     =      0x40000
 *   Property.atu_dma_base =   0x2a040000 -- 'atu_dma'
 *   Property.atu_dma_size =      0x40000
 *   Property.regs_base    =   0x2a080000 -- 'dbi'
 *   Property.regs_size    =      0x40000
 *   -- Must be a region where PCI bus address == CPU address!
 *   Property.mem_base     = 0x3240000000 -- ranges 64-bit
 *   Property.mem_size     =  0x2e8000000
 *   Property.bus_base     = 0x3240000000
 *   Property.lanes        =            2 -- 'num-lanes'
 *   Property.max_link_speed =          3 -- 'max-link-speed'
 *   -- Number of Fast Training Sequence ordered sets for Gen1
 *   Property.nft_gen1     =           52 -- source: tegra234
 *   -- Number of Fast Training Sequence ordered sets for Gen2+
 *   Property.nft_gen2     =           80 -- source: tegra234
 *   Property.gen4_preset_vec =     0x340 -- source: tegra234
 *   Property.cid          =            8
 *
 *   -- PU-0
 *   Property.pu0_base     =    0x3f40000 -- pu-0
 *   Property.pu0_size     =      0x10000
 *   Property.pu1_base     =    0x3f50000 -- pu-1
 *   Property.pu1_size     =      0x10000
 *
 *   -- GPIO for AON cluster
 *   Property.gpio_aon_base =   0xc2f1000 -- gpio_aon
 *   Property.gpio_aon_size =      0x1000
 *
 *   -- PINMUX for AON cluster
 *   Property.pinmux_aon_base = 0xc300000
 *   Property.pinmux_aon_size =    0x4000
 *
 *   -- HSP device (hardware synchronization primitives)
 *   Property.hsp_base     =    0x3c00000 -- hsp
 *   Property.hsp_size     =      0xa0000
 *
 *   -- BPMP device
 *   Property.bpmp_tx_base =   0x40070000 -- bpmp sram (cpu-bpmp-rx)
 *   Property.bpmp_tx_size =       0x1000
 *   Property.bpmp_rx_base =   0x40071000 -- bpmp sram (cpu-bpmp-rx)
 *   Property.bpmp_rx_size =       0x1000
 *  end)
 * \endcode
 */

#include "dwc_pcie_core.h"

#include <l4/cxx/utils>
#include <l4/re/error_helper>
#include <l4/drv/tegra/bpmp/bpmp_base.h>
#include <l4/drv/tegra/pwm/pwm_base.h>

#include <pci-cfg.h>

namespace {

/**
 * GPIO controller in the VDD_RTC core power (AON cluster).
 *
 * It has 1 bank with 6 ports.
 */
struct Tegra_gpio_aon
{
  enum Pin_regs
  {
    Enable_config = 0x00,
      Enable_config_enable = 1 << 0,
      Enable_config_out = 1 << 1,
      Enable_timestamp_func = 1 << 7,
    Debounce_control = 0x04,
    Input = 0x08,
    Output_control = 0x0c,
      Output_control_floated = 1 << 0,
    Output_value = 0x10,
      Output_value_high = 1 << 0,
    Interrupt_clear = 0x14,
  };

  [[nodiscard]] int init(l4_uint64_t base, l4_uint64_t size)
  {
    if (l4_addr_t va = res_map_iomem(base, size))
      {
        regs = new L4drivers::Mmio_register_block<32>(va);
        d_printf(DBG_INFO, "Initialize gpio-aon@%llx.\n", base);

        unsigned offs = offset(0, 4, 5);
        l4_uint32_t old = regs[offs + Enable_config].read();
        regs[offs + Enable_config].set(Enable_timestamp_func);
        d_printf(DBG_DEBUG, "gpio_aon: Enable_config %08x => %08x\n",
                 old, regs[offs + Enable_config].read());
        return L4_EOK;
      }

    return -L4_ENOMEM;
  }

  l4_uint32_t offset(unsigned bank, unsigned port, unsigned pin)
  {
    if (bank > 0)
      L4Re::throw_error_fmt(-L4_EINVAL, "Invalid GPIO bank %u", bank);
    if (port >= 6)
      L4Re::throw_error_fmt(-L4_EINVAL, "Invalid GPIO port %u", port);
    if (pin >= 16)
      L4Re::throw_error_fmt(-L4_EINVAL, "Invalid GPIO pin %u", pin);
    return bank * 0x1000 + port * 0x200 + pin * 0x20;
  }

  void set(unsigned bank, unsigned port, unsigned pin, bool level)
  {
    unsigned offs = offset(bank, port, pin);
    l4_uint32_t r = regs[offs + Output_value];
    regs[offs + Output_value].modify(1 << 0, !!level * Output_value_high);
    if (0)
      printf("GPIO Set: %04x: %04x => %04x\n",
             offs + Output_value, r, regs[offs + Output_value].read());
  }

  void direction(unsigned bank, unsigned port, unsigned pin, bool output)
  {
    unsigned offs = offset(bank, port, pin);
    l4_uint32_t r1 = regs[offs + Output_control];
    l4_uint32_t r2 = regs[offs + Enable_config];
    regs[offs + Output_control].modify(Output_control_floated,
                                       !output * Output_control_floated);
    regs[offs + Enable_config].modify(Enable_config_out,
                                      !!output * Enable_config_out
                                      | Enable_config_enable);
    if (0)
      printf("GPIO Direction: %04x: %04x => %04x, %04x: %04x => %04x\n",
             offs + Output_control, r1, regs[offs + Output_control].read(),
             offs + Enable_config, r2, regs[offs + Enable_config].read());
  }

  L4drivers::Register_block<32> regs;
  L4Re::Util::Dbg log{4, "tegra", "gpio-aon"}; // only for LOG_DWC_MMIO_ACCESS
};

struct Tegra_pinmux_aon
{
  [[nodiscard]] int init(l4_uint64_t base, l4_uint64_t size)
  {
    if (l4_addr_t va = res_map_iomem(base, size))
      {
        regs = new L4drivers::Mmio_register_block<32>(va);
        d_printf(DBG_INFO, "Initialize pinmux-aon@%llx.\n", base);
        return L4_EOK;
      }

    return -L4_ENOMEM;
  }

  void gpio_request_enable()
  {
    unsigned reg = 0x3028;
    unsigned bit = 10;
    regs[reg].clear(1 << bit);
  }

  L4drivers::Register_block<32> regs;
  L4Re::Util::Dbg log{4, "tegra", "pixmux-aon"}; // only for LOG_DWC_MMIO_ACCESS
};

class Tegra_hsp : public Tegra_hsp_base<Tegra_hsp>
{
public:
  static constexpr bool Tbuf_log = false;

  [[nodiscard]] int init(l4_uint64_t base, l4_uint64_t size)
  {
    if (l4_addr_t va = res_map_iomem(base, size))
      {
        _regs = new L4drivers::Mmio_register_block<32>(va);
        return Tegra_hsp_base::init(va);
      }

    return -L4_ENODEV;
  }

  L4Re::Util::Dbg log{4, "tegra", "hsp"}; // only for LOG_DWC_MMIO_ACCESS
};

class Tegra_bpmp : public Tegra_bpmp_base<Tegra_bpmp, Tegra_hsp>
{
public:
  using Tegra_bpmp_base::Tegra_bpmp_base;

  [[nodiscard]]
  int init(l4_uint64_t phys_tx, l4_uint64_t size_tx,
           l4_uint64_t phys_rx, l4_uint64_t size_rx)
  {
    if (l4_addr_t va_tx = res_map_iomem(phys_tx, size_tx))
      if (l4_addr_t va_rx = res_map_iomem(phys_rx, size_rx))
        return Tegra_bpmp_base::init(va_tx, size_tx, va_rx, size_rx);

    return -L4_ENODEV;
  }

  // logging in libbpmp, disabled by default
  L4Re::Util::Dbg chan_log1{4, "tegra", "bpmp"};
  L4Re::Util::Dbg chan_log2{4, "tegra", "bpmp"};
  L4Re::Util::Dbg chan_log3{4, "tegra", "bpmp"};
  L4Re::Util::Dbg chan_log4{4, "tegra", "bpmp"};
};

class Tegra_pwm : public Tegra_pwm_base<Tegra_pwm, Tegra_bpmp>
{
public:
  using Tegra_pwm_base::Tegra_pwm_base;

  [[nodiscard]] int init(l4_uint64_t mmio_base, l4_uint64_t mmio_size)
  {
    if (l4_addr_t va = res_map_iomem(mmio_base, mmio_size))
      {
        _regs = new L4drivers::Mmio_register_block<32>(va);
        return L4_EOK;
      }

    return -L4_ENODEV;
  }

  // logging in libpwm, disabled by default
  L4Re::Util::Dbg log{4, "tegra", "pwm"};
};

class Pcie_tegra194_bridge : public Dwc_pcie
{
  using Clk_id = Tegra_bpmp::Mrq_clk::Clk_id;
  using Reset_id = Tegra_bpmp::Mrq_reset::Reset_id;

public:
  explicit Pcie_tegra194_bridge(int segment = 0, unsigned bus_nr = 0);
  void init() override;
  void enable_phy(L4drivers::Register_block<32> &phy);

  // Root port application registers (_appl)
  enum Appl : unsigned
  {
    Pinmux = 0x00,
    Ctrl = 0x04,
    Intr_en_l0 = 0x08,
    Intr_status_l0 = 0x0c,
    Fault_en_l0 = 0x10,
    Fault_status_l0 = 0x14,
    Fault_en_l1 = 0x18,
    Intr_en_l1 = 0x1c,
    Intr_status_l1_0 = 0x20,
    Intr_status_l1_1 = 0x2c,
    Intr_status_l1_2 = 0x30,
    Intr_status_l1_3 = 0x34,
    Intr_status_l1_6 = 0x3c,
    Intr_status_l1_7 = 0x40,
    Intr_en_l1_8 = 0x44,
    Intr_status_l1_8 = 0x4c,
    Intr_status_l1_9 = 0x54,
    Intr_status_l1_10 = 0x58,
    Intr_status_l1_11 = 0x64,
    Intr_status_l1_13 = 0x74,
    Intr_status_l1_14 = 0x78,
    Intr_status_l1_15 = 0x7c,
    Intr_status_l1_17 = 0x88,
    Msi_ctrl_2 = 0xb0,

    Dm_type = 0x100,
    Cfg_base_addr = 0x104,
    Cfg_iatu_dma_base_addr = 0x108,
    Cfg_misc = 0x110,
    Cfg_slcg_override = 0x114,
  };

  enum Cfg : unsigned
  {
  };

  // Data Bus Interface registers (_regs) related to PCIe Tegra194
  enum Dbi : unsigned
  {
    Cap_spcie_cap_off = 0x154,
  };

  enum Phy : unsigned
  {
    Control_gen1 = 0x78,
    Rx_debounce_time = 0xa4,
    Periodic_eq_ctrl_gen3 = 0xc0,
    Periodic_eq_ctrl_gen4 = 0xc4,
    Dir_search_ctrl = 0xd4,
    Rx_margin_sw_int_en = 0xe0,
    Rx_margin_cya_ctrl = 0xf8,
  };

  unsigned int_map(unsigned i) const { return _int_map[i]; }

private:
  bool do_init();
  // Called by Dwc_pcie::host_init().
  bool controller_host_init() override;

  void dbg_dump_appl();
  void dbg_dump_dbi();

  L4drivers::Register_block<32> _appl;    ///< APB: application registers
  L4drivers::Register_block<32> _phy0;    ///< PHY0 registers
  L4drivers::Register_block<32> _phy1;    ///< PHY1 registers

  Tegra_hsp hsp;
  Tegra_bpmp bpmp{&hsp};
  Tegra_pwm pwm{&bpmp};
  Tegra_gpio_aon gpio_aon;
  Tegra_pinmux_aon pinmux_aon;

  Int_property _appl_base{~0};
  Int_property _appl_size{~0};
  Int_property _bpmp_tx_base{~0};
  Int_property _bpmp_tx_size{~0};
  Int_property _bpmp_rx_base{~0};
  Int_property _bpmp_rx_size{~0};
  Int_property _hsp_base{~0};
  Int_property _hsp_size{~0};
  Int_property _gpio_aon_base{~0};
  Int_property _gpio_aon_size{~0};
  Int_property _pinmux_aon_base{~0};
  Int_property _pinmux_aon_size{~0};
  Int_property _phy0_base{~0};
  Int_property _phy0_size{~0};
  Int_property _phy1_base{~0};
  Int_property _phy1_size{~0};
  Int_property _gen4_preset_vec{0x340};
  Int_property _cid{~0};

  Int_property _int_map[4]/*{~0, ~0, ~0, ~0}*/;

  // log register access -- only for LOG_DWC_MMIO_ACCESS
  L4Re::Util::Dbg log_appl{4, "tegra", "pcie/appl"};
  L4Re::Util::Dbg log_phy{4, "tegra", "phy"};
};

class Irq_router_rs : public Resource_space
{
public:
  char const *res_type_name() const override
  { return "Tegra PCIe IRQ router"; }

  bool request(Resource *parent, ::Device *pdev,
               Resource *child, ::Device *cdev) override
  {
    auto *cd = dynamic_cast<Hw::Device *>(cdev);
    if (!cd)
      return false;

    unsigned pin = child->start();
    if (pin > 3)
      return false;

    auto *pd = dynamic_cast<Pcie_tegra194_bridge *>(pdev);
    if (!pd)
      return false;

    unsigned slot = (cd->adr() >> 16);
    unsigned map_idx = (pin + slot) & 3;
    if (pd->int_map(map_idx) == static_cast<unsigned>(~0))
      return false;

    unsigned irq_nr = pd->int_map(map_idx);

    d_printf(DBG_DEBUG, "%s/%08x: Requesting IRQ%c at slot %d => IRQ %d\n",
             cd->get_full_path().c_str(), cd->adr(),
             (int)('A' + pin), slot, irq_nr);

    child->del_flags(Resource::F_relative);
    child->start(irq_nr);
    child->del_flags(Resource::Irq_type_mask);
    child->add_flags(Resource::Irq_type_base | Resource::Irq_type_level_high);

    child->parent(parent);

    return true;
  }

  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to Irq_router_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Irq_router_rs\n");
    return false;
  }
};

Pcie_tegra194_bridge::Pcie_tegra194_bridge(int segment, unsigned bus_nr)
: Dwc_pcie(segment, bus_nr, this, nullptr)
{
  register_property("appl_base", &_appl_base);
  register_property("appl_size", &_appl_size);
  register_property("bpmp_tx_base", &_bpmp_tx_base);
  register_property("bpmp_tx_size", &_bpmp_tx_size);
  register_property("bpmp_rx_base", &_bpmp_rx_base);
  register_property("bpmp_rx_size", &_bpmp_rx_size);
  register_property("hsp_base", &_hsp_base);
  register_property("hsp_size", &_hsp_size);
  register_property("gpio_aon_base", &_gpio_aon_base);
  register_property("gpio_aon_size", &_gpio_aon_size);
  register_property("pinmux_aon_base", &_pinmux_aon_base);
  register_property("pinmux_aon_size", &_pinmux_aon_size);
  register_property("pu0_base", &_phy0_base);
  register_property("pu0_size", &_phy0_size);
  register_property("pu1_base", &_phy1_base);
  register_property("pu1_size", &_phy1_size);
  register_property("gen4_preset_vec", &_gen4_preset_vec);
  register_property("cid", &_cid);

  register_property("irq_pin_a", &_int_map[0]); // optional
  register_property("irq_pin_b", &_int_map[1]); // optional
  register_property("irq_pin_c", &_int_map[2]); // optional
  register_property("irq_pin_d", &_int_map[3]); // optional

  set_name_if_empty("pcie_tegra194");
}

void Pcie_tegra194_bridge::init()
{
  static_cast<void>(do_init());
}

bool Pcie_tegra194_bridge::do_init()
{
  if (   assert_property(&_appl_base, "appl_base", ~0)
      || assert_property(&_appl_size, "appl_size", ~0)
      || assert_property(&_bpmp_tx_base, "bpmp_tx_base", ~0)
      || assert_property(&_bpmp_tx_size, "bpmp_tx_size", ~0)
      || assert_property(&_bpmp_rx_base, "bpmp_rx_base", ~0)
      || assert_property(&_bpmp_rx_size, "bpmp_rx_size", ~0)
      || assert_property(&_hsp_base, "hsp_base", ~0)
      || assert_property(&_hsp_size, "hsp_size", ~0)
      || assert_property(&_gpio_aon_base, "gpio_aon_base", ~0)
      || assert_property(&_gpio_aon_size, "gpio_aon_size", ~0)
      || assert_property(&_pinmux_aon_base, "pinmux_aon_base", ~0)
      || assert_property(&_pinmux_aon_size, "pinmux_aon_size", ~0)
      || assert_property(&_phy0_base, "pu0_base", ~0)
      || assert_property(&_phy0_size, "pu0_size", ~0)
      || assert_property(&_phy1_base, "pu1_base", ~0)
      || assert_property(&_phy1_size, "pu1_size", ~0)
      || assert_property(&_gen4_preset_vec, "gen4_preset_vec", ~0)
      || assert_property(&_cid, "cid", ~0)
      )
    return false;

  // Hardware synchronization primitives equired by Tegra_bpmp.
  if (hsp.init(_hsp_base, _hsp_size) < 0)
    {
      error("cannot initialize 'hsp' device.");
      return false;
    }

  // Boot and power management processor (mailbox driver).
  if (bpmp.init(_bpmp_tx_base, _bpmp_tx_size, _bpmp_rx_base, _bpmp_rx_size) < 0)
    {
      error("cannot initialize 'bpmp' device.");
      return false;
    }

  // GPIO for AON cluster.
  if (gpio_aon.init(_gpio_aon_base, _gpio_aon_size) < 0)
    {
      error("cannot initialize 'gpio-aon' device.");
      return false;
    }

  // Pinmux for AON cluster.
  if (pinmux_aon.init(_pinmux_aon_base, _pinmux_aon_size) < 0)
    {
      error("cannot initialize 'pixmum_aon' device.");
      return false;
    }

  if (pwm.init(0x32a0000, 0x10000) < 0)
    {
      error("cannot initialize 'pwm' device.");
      return false;
    }

  // Activate FAN
  pwm.fan_enable();

#ifdef LOG_DWC_MMIO_ACCESS
  L4Re::Util::Dbg::set_level(1);
#endif

  if (bpmp.req_ping(0x01010101) < 0)
    {
      error("ping failed.");
      return false;
    }

  bool supported_fw_tag = bpmp.req_supported(Tegra_bpmp::Mrq_query_fw_tag::Mrq);
  if (supported_fw_tag)
    bpmp.req_query_fw_tag();

  // list all clocks
  if (0)
    {
      int max_clk_id = bpmp.req_clk_get_max();
      for (int i = 0; i < max_clk_id; ++i)
        {
          using Rsp = Tegra_bpmp::Mrq_clk::Rsp;
          Tegra_bpmp::Mrq_clk::Rsp::Get_all_info info;
          if (bpmp.req_clk_get_all_info(i, info) >= 0)
            {
              auto *name = reinterpret_cast<char const *>(&info.name);
              char buf[128];
              int r = snprintf(
                buf, sizeof(buf),
                "  name(%#04x) %.*s, flags=%c%c%c, parents=%02x",
                i, static_cast<int>(strlen(name)), name,
                info.flags & Rsp::Get_all_info::Flags::Has_mux ? 'm' : '-',
                info.flags & Rsp::Get_all_info::Flags::Has_set_rate ? 's' : '-',
                info.flags & Rsp::Get_all_info::Flags::Is_root ? 'r' : '-',
                info.parent);
              if (r < 0)
                continue;
              unsigned len = r;
              for (unsigned j = 0; j < info.num_parents; ++j)
                if (info.parents[j] != info.parent)
                  if (len < sizeof(buf))
                    {
                      r = snprintf(buf + len, sizeof(buf) - len, ",%02x",
                                   info.parents[j]);
                      if (r < 0)
                        continue;
                      len += r;
                    }
              printf("%s\n", buf);
            }
        }
    }

  // list all power gates
  if (0)
    {
      int max_pg_id = bpmp.req_pg_get_max();
      for (int i = 0; i < max_pg_id; ++i)
        {
          Tegra_bpmp::Mrq_pg::Rsp::Get_name info;
          if (bpmp.req_pg_get_name(i, info) >= 0)
            {
              auto *name = reinterpret_cast<char const *>(&info.name);
              printf("  name(%#04x) %.*s\n",
                     i, static_cast<int>(strlen(name)), name);
            }
        }
    }

  gpio_aon.set(0, 4, 5, 0);
  gpio_aon.direction(0, 4, 5, true);

  // Power-on the PCIe controller.
  // "power-domains = <&bpmp TEGRA234_POWER_DOMAIN_PCIEX4CA>;"
  if (bpmp.req_pg_set_state(
       Tegra_bpmp::Mrq_pg::Tegra234_power_domain_pciex4ca, 1) < 0)
    {
      error("could not power-on PCIe controller.");
      return false;
    }

  // Set PCIe controller state.
  if (_cid != 8)
    d_printf(DBG_WARN, "\033[31mpcie_id %lld != 8\033[m\n", _cid.val());
  if (bpmp.req_uphy_set_pcie_controller_state(
       0, static_cast<l4_uint8_t>(_cid), 1) < 0)
    {
      error("could not set PCIe controller state.");
      return false;
    }

  pinmux_aon.gpio_request_enable();

  gpio_aon.set(0, 4, 5, 1);

  // Enable clock.
  // "clocks = <&bpmp TEGRA234_CLK_PEX2_C8_CORE>;"
  if (bpmp.req_clk_enable(Clk_id::Tegra234_clk_pex2_c8_core) < 0)
    {
      error("failed to enable TEGRA234_CLK_PEX2_C8_CORE.");
      return false;
    }

  // Reset.
  // "resets = <&bpmp TEGRA234_RESET_PEX2_CORE_8_APB>,
  if (bpmp.req_reset_deassert(Reset_id::Tegra234_reset_pex2_core_8_apb) < 0)
    {
      error("failed to reset TEGRA234_RESET_PEX2_CORE_8_APB.");
      return false;
    }

  if (l4_addr_t va = res_map_iomem(_appl_base, _appl_size))
    _appl = new L4drivers::Mmio_register_block<32>(va);
  else
    {
      error("could not map IP core memory.");
      return false;
    }

  if (l4_addr_t va = res_map_iomem(_phy0_base, _phy0_size))
    _phy0 = new L4drivers::Mmio_register_block<32>(va);
  else
    {
      error("could not map PHY 0 memory.");
      return false;
    }

  if (l4_addr_t va = res_map_iomem(_phy1_base, _phy1_size))
    _phy1 = new L4drivers::Mmio_register_block<32>(va);
  else
    {
      error("could not map PHY 1 memory.");
      return false;
    }

  // HW_HOT_RST_MODE=2 (IMDT_RST_LTSSM_EN), old was probably 1
  // HW_HOT_RST_EN=1
  _appl[Appl::Ctrl].modify(3 << 22, 2 << 22 | 1 << 20);

  enable_phy(_phy0);
  enable_phy(_phy1);

  if (_regs_base > 0x0'ffff'ffffLL)
    {
      error("DBI address beyond 4 GiB not supported!");
      return false;
    }

  // Update CFG base address.
  _appl[Appl::Cfg_base_addr] = _regs_base & 0xffff'f000;

  // configure this core for RP mode operation
  _appl[Appl::Dm_type] = 4;

  // CYA: bypass for second level clock gating. Master switch to disable.
  _appl[Appl::Cfg_slcg_override] = 0;

  // Indicate that a card is present in the slot.
  _appl[Appl::Ctrl].set(1 << 6);

  // Set cache attributes for AXI interface for DBB.
  _appl[Appl::Cfg_misc].set(3 << 10);

  // Doesn't support clkreq:
  // - Set CLKREQ_OVERRIDE_EN=1 + set PEX_CLKREQ_I_N = 0
  // - Clear CLKREQ_DEFAULT_VALUE.
  _appl[Appl::Pinmux].modify(1 << 3 | 1 << 13, 1 << 2);

  if (bpmp.req_reset_deassert(Reset_id::Tegra234_reset_pex2_core_8) < 0)
    {
      error("could not reset TEGRA234_RESET_PEX2_CORE_8.");
      return false;
    }

  if (int ret = Dwc_pcie::host_init(); ret < 0)
    return ret;

  if (!_iatu_unroll_enabled)
    {
      error("iATU unroll mode not available.");
      return false;
    }

  if (!Dwc_pcie::setup_rc())
    return false;

  auto const *kip = l4re_kip();
  auto start_wait = l4_kip_clock(kip);
  unsigned i;
  for (i = 0; i < 10; ++i)
    {
      // PCIe capability: Link Status Register
      if (!(_regs.r<16>(_offs_cap_pcie + 0x12) & (1 << 13)))
        {
          // clear PEX_RST
          _appl[Appl::Pinmux].clear(1 << 0);
          l4_ipc_sleep_us(500);
          // set APP_LTSSM_EN=1
          _appl[Appl::Ctrl].set(1 << 7);
          // set PEX_RST
          _appl[Appl::Pinmux].set(1 << 0);
          l4_ipc_sleep_ms(100);
          // PCIe capability: Link Status Register
          if (_regs.r<16>(_offs_cap_pcie + 0x12) & (1 << 13))
            {
              d_printf(DBG_WARN, "Link up after %llu ms.\n",
                       (l4_kip_clock(kip) - start_wait) / 1000);
              break;
            }
        }
      l4_ipc_sleep_ms(200);
    }

  if (i >= 10)
    {
      error("Link still down after %llu ms!\n",
            (l4_kip_clock(kip) - start_wait) / 1000);
      return false;
    }

  l4_uint32_t rate;
  // PCIe capability: Link Status Register
  switch (_regs.r<16>(_offs_cap_pcie + 0x12) & 0xf)
    {
    default: rate =  62'500000; break; // default/unknown
    case 1:  rate =  62'500000; break; // Gen1
    case 2:  rate = 125'000000; break; // Gen2
    case 3:  rate = 250'000000; break; // Gen3
    case 4:  rate = 500'000000; break; // Gen4
    }

  // Set clock rate.
  // "clocks = <&bpmp TEGRA234_CLK_PEX2_C8_CORE>;"
  if (bpmp.req_clk_set_rate(Clk_id::Tegra234_clk_pex2_c8_core, rate) < 0)
    {
      error("cannot set clock rate of TEGRA234_CLK_PEX2_C8_CORE to %u.", rate);
      return false;
    }

  _appl[Appl::Intr_status_l0] = ~0U;
  _appl[Appl::Intr_status_l1_0] = ~0U;
  _appl[Appl::Intr_status_l1_1] = ~0U;
  _appl[Appl::Intr_status_l1_2] = ~0U;
  _appl[Appl::Intr_status_l1_3] = ~0U;
  _appl[Appl::Intr_status_l1_6] = ~0U;
  _appl[Appl::Intr_status_l1_7] = ~0U;
  _appl[Appl::Intr_status_l1_8] = ~0U;
  _appl[Appl::Intr_status_l1_9] = ~0U;
  _appl[Appl::Intr_status_l1_10] = ~0U;
  _appl[Appl::Intr_status_l1_11] = ~0U;
  _appl[Appl::Intr_status_l1_13] = ~0U;
  _appl[Appl::Intr_status_l1_14] = ~0U;
  _appl[Appl::Intr_status_l1_15] = ~0U;
  _appl[Appl::Intr_status_l1_17] = ~0U;

  _appl[Appl::Intr_en_l0].set(1 << 0); // LINK_STATE_INT_EN

  // // PCIe capability: Link Control Register
  _regs.r<16>(_offs_cap_pcie + 0x10).set(1 << 10); // PCI_EXP_LNKCTL_LBMIE

  _appl[Appl::Intr_en_l0].modify(        0
                                 | 1 << 31  // SYS_MSI_INTR_EN=0
                                 ,
                                         0
                                 | 1 << 30 // SYS_INTR_EN=1
                                 | 1 <<  8 // INT_INT_EN=1
                                );
  // - We must set INTX_EN, otherwise attached devices don't receive interrupts.
  // - We must NOT set CFG_BW_MGT_INT_EN.
  _appl[Appl::Intr_en_l1_8] = 0
                            | 1 << 11 // INTX_EN
                            ;

  if (0)
    dbg_dump_appl();
  if (0)
    dbg_dump_dbi();

  typedef Hw::Pci::Irq_router_res<Irq_router_rs> Irq_res;
  Irq_res *ir = new Irq_res();
  ir->set_id("IRQR");
  add_resource_rq(ir);

  // set PCI_EXP_DEVCTL2_LTR_EN
  _regs[0x98].set(1 << 10);
  // set PCI_EXP_RTCTL_CRSSVE
  _regs.r<16>(0x8c).set(1 << 4);
  // Set PME status?
  _regs[0x44].set(1 << 15);
  _regs.r<16>(0x80).set(1 << 6);
  _regs.r<16>(0x80).set(1 << 5);
  _regs.r<16>(0x80).set(1 << 6);
  _regs.r<16>(0x3e).write(0x2);

  // disable prefetchable memory
  _regs.r<32>(0x24).write(0x0000fff0);

  discover_bus(this, this);
  Hw::Device::init();

  d_printf(DBG_INFO, "bridge:\n"
           "  0x10: %08x (BAR0), 0x14: %08x (BAR1)\n"
           "  0x1c: %04x (I/O base/limit)\n"
           "  0x20: %08x (memory)\n"
           "  0x24: %08x (pref memory\n"
           "  0x28: %08x (pref memory base upper 32-bit)\n"
           "  0x2c: %08x (pref memory limit upper 32-bit)\n"
           "  0x30: %08x (I/O limit/base upper 32-bit)\n",
           _regs[0x10].read(), _regs[0x14].read(), _regs.r<16>(0x1c).read(),
           _regs[0x20].read(), _regs[0x24].read(),
           _regs[0x28].read(), _regs[0x2c].read(),
           _regs[0x30].read());

  set_iatu_region(Iatu_vp::Idx0, _cfg_base, _cfg_size, 0x01000000, Tlp_type::Cfg0);
  d_printf(DBG_INFO, "device:\n"
           "  0x10: %08x, 0x14: %08x (BAR0, BAR1)\n"
           "  0x18: %08x, 0x1c: %08x (BAR2, BAR3)\n"
           "  0x20: %08x, 0x24: %08x (BAR4, BAR5)\n",
           _cfg[0x10].read(), _cfg[0x14].read(), _cfg[0x18].read(),
           _cfg[0x1c].read(), _cfg[0x20].read(), _cfg[0x24].read());

  return true;
}

bool Pcie_tegra194_bridge::controller_host_init()
{
  // Set physical iATU DMA base registers offset.
  // XXX 4 GiB LIMIT!
  _appl[Appl::Cfg_iatu_dma_base_addr] = _atu_base & 0xfffc'0000;

  // clear IO_BASE_IO_DECODE (bit 0) and IO_BASE_IO_DECODE_BIT8 (bit 8)
  _regs[Hw::Pci::Config::Io_base].modify(1 << 8 | 1 << 0, 0);

  // set CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE (bit 0) and
  // CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE (bit 16)
  _regs[Hw::Pci::Config::Pref_mem_base].set(1 << 16 | 1 << 0);

  _regs[Hw::Pci::Config::Bar_0] = 0;

  // enable 0xffff0001 response for CRS
  _regs[Port_logic::Amba_err_rsp_dflt].modify(3 << 3, 2 << 3);

  // PCIe capability: Link Capabilities Register -- maximum link width
  _regs[_offs_cap_pcie + 0xc].modify(0x3f0, _num_lanes << 4);

  l4_uint16_t offs_cap_pl_16gt = get_pci_ext_cap_offs(Hw::Pci::Extended_cap::Pl_16gt);
  for (unsigned i = 0; i < _num_lanes; ++i)
    {
      _regs.r<16>(Dbi::Cap_spcie_cap_off + 2 * i).modify(0xf0f, 0x505);
      // PCI_EXT_CAP_ID_PL_16GT: PCI_PL_16GT_LE_CTRL
      _regs.r<8>(offs_cap_pl_16gt + 0x20 + i).modify(0xff, 0x55);
    }

  _regs[Port_logic::Gen3_related_off].clear(3 << 24);
  _regs[Port_logic::Gen3_eq_control_off]
    .modify(0xffff << 8 | 0xf << 0, 0x3ff << 8);
  _regs[Port_logic::Gen3_related_off].modify(3 << 24, 1 << 24);
  _regs[Port_logic::Gen3_eq_control_off]
    .modify(0xffff << 8 | 0xf << 0, _gen4_preset_vec << 8);
  _regs[Port_logic::Gen3_related_off].clear(3 << 24);

  // PCI_EXT_CAP_ID_VNDR: PCIE_RAS_DES_EVENT_COUNTER_CONTROL
  l4_uint16_t offs_ext_cap_vndr = get_pci_ext_cap_offs(Hw::Pci::Extended_cap::Vsec);
  // Set EVENT_COUNTER_ENABLE_ALL (bits 2..4) and EVENT_COUNTER_GROUP_5
  // (bit 24..26)
  _regs[offs_ext_cap_vndr + 0x8].write(7 << 2 | 5 << 24);

  // Set values from aspm-cmrt-us=0x3c / aspm-pwr-on-t-us=0x14
  l4_uint16_t offs_ext_cap_l1ss = get_pci_ext_cap_offs(Hw::Pci::Extended_cap::L1ss);
  // PCI_EXT_CAP_ID_L1SS: PCI_L1SS_CAP
  _regs[offs_ext_cap_l1ss + 0x4].modify(0xff00 | 0xf8'0000, 0x3c << 8 | 0x14 << 19);

  // aspm-l0s-entrance-latency-us=0x3
  // tegra_pcie_dw_of_data.aspm_l1_enter_lat=0x4
  _regs[Port_logic::Afr].modify(7 << 27 | 7 << 24, 1 << 30 | 4 << 27 | 3 << 24);

  // disable ASPM L1.1 + L1.2
  // PCI_EXT_CAP_ID_L1SS: PCI_L1SS_CAP
  _regs[offs_ext_cap_l1ss + 0x4].clear(1 << 3 | 1 << 2);

  if (0)
    {
      l4_uint32_t rate = 500'000'000ULL;
      if (bpmp.req_clk_round_rate(
            Tegra_bpmp_if::Mrq_clk::Clk_id::Tegra234_clk_pex2_c8_core, rate) < 0)
        {
          error("failed to round rate of TEGRA234_CLK_PEX2_C8_CORE.");
          return false;
        }
    }

  return true;
}

void Pcie_tegra194_bridge::enable_phy(L4drivers::Register_block<32> &phy)
{
  // tegra234: eios_override = false
  phy[Control_gen1].set(1 << 2);
  phy[Periodic_eq_ctrl_gen3].modify(1 << 0, 1 << 1);
  phy[Periodic_eq_ctrl_gen4].set(1 << 1);
  phy[Rx_debounce_time].modify(0xffff, 160);
  // tegra234: one_dir_search = true
  phy[Dir_search_ctrl].clear(1 << 18);
  // tegra234: lane_margin = true
  phy[Rx_margin_sw_int_en] = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3;
  phy[Rx_margin_cya_ctrl].set(1 << 0 | 1 << 1);
}

[[maybe_unused]]
void Pcie_tegra194_bridge::dbg_dump_appl()
{
  printf("\n**APPL**\n");
  for (unsigned i = 0; i < 0x194; i += 4)
    {
      if ((i % 32) == 0)
        printf("%04x: ", i);
      if (i == 0x10c)
        printf("........ ");
      else
        printf("%08x ", _appl[i].read());
      if ((i % 32) == 32 - 4)
        printf("\n");
    }
  putchar('\n');
}

[[maybe_unused]]
void Pcie_tegra194_bridge::dbg_dump_dbi()
{
  printf("**DBI**\n");
  for (unsigned i = 0; i < 0x1000; i += 4)
    {
      if ((i % 32) == 0)
        printf("%04x: ", i);
      printf("%08x ", _regs[i].read());
      if ((i % 32) == 32 - 4)
        printf("\n");
    }
  putchar('\n');
}

class Pcie_tegra194 : public Pcie_tegra194_bridge
{};

static Hw::Device_factory_t<Pcie_tegra194> f("Pcie_tegra194");

}
