/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 * Driver for the PCIe controller on i.MX8M Plus boards.
 *
 * The controller is a Designware core. Everything around it is i.MX8M Plus
 * specific and is driven here by direct register access: the clock roots and
 * gates in the CCM, the HSIO blk-ctrl, the PCIe PHY and the reset signals in
 * the SRC.
 *
 * The HSIOMIX and PCIE_PHY power domains are the exception. Their PGC control
 * registers in the GPC are not accessible from the normal world, so they are
 * switched through the SiP service of the firmware. That needs an 'arm_smc'
 * capability and a kernel that permits the SiP call range (0x02000000).
 *
 * The device tree entries this driver replaces look like this:
 * \code{.dts}
 * pcie: pcie@33800000 {
 *        compatible = "fsl,imx8mp-pcie";
 *        reg = <0x33800000 0x400000>, <0x1ff00000 0x80000>;
 *        reg-names = "dbi", "config";
 *        ranges = <0x81000000 0 0x00000000 0x1ff80000 0 0x00010000>,
 *                 <0x82000000 0 0x18000000 0x18000000 0 0x07f00000>;
 *        num-lanes = <1>;
 *        interrupts = <GIC_SPI 140 IRQ_TYPE_LEVEL_HIGH>;
 *        interrupt-names = "msi";
 *        interrupt-map = <0 0 0 1 &gic 0 126 4>,
 *                        <0 0 0 2 &gic 0 125 4>,
 *                        <0 0 0 3 &gic 0 124 4>,
 *                        <0 0 0 4 &gic 0 123 4>;
 *        fsl,max-link-speed = <3>;
 *        resets = <&src IMX8MP_RESET_PCIE_CTRL_APPS_EN>,
 *                 <&src IMX8MP_RESET_PCIE_CTRL_APPS_TURNOFF>;
 *        phys = <&pcie_phy>;
 * };
 *
 * pcie_phy: pcie-phy@32f00000 {
 *        compatible = "fsl,imx8mp-pcie-phy";
 *        reg = <0x32f00000 0x10000>;
 *        resets = <&src IMX8MP_RESET_PCIEPHY>,
 *                 <&src IMX8MP_RESET_PCIEPHY_PERST>;
 *        power-domains = <&hsio_blk_ctrl IMX8MP_HSIOBLK_PD_PCIE_PHY>;
 *        fsl,clkreq-unsupported;
 *        fsl,refclk-pad-mode = <IMX8_PCIE_REFCLK_PAD_INPUT>;
 * };
 * \endcode
 *
 * The corresponding io.cfg code:
 * \code{.lua}
 * pcie = Io.Hw.Pcie_imx8mp(function()
 *   Property.perst_gpio = hw.gpio1  -- GPIO2 in device tree numbering
 *   Property.perst_pin  = 7
 *   Property.refclk_gpio  = hw.gpio3 -- output enable of the clock generator
 *   Property.refclk_pin   = 22
 *   Property.refclk_level = 1
 *   Property.regs_base  = 0x33800000 -- reg: dbi
 *   Property.regs_size  = 0x00400000
 *   Property.cfg_base   = 0x1ff00000 -- reg: config
 *   Property.cfg_size   = 0x00080000
 *   Property.atu_base   = 0x33b00000 -- reg: atu of the pcie-ep node
 *   Property.atu_size   = 0x00100000
 *   Property.mem_base   = 0x18000000 -- ranges: 32 bit non-prefetchable memory
 *   Property.mem_size   = 0x07f00000
 *   Property.irq_pin_a  = 32 + 126
 *   Property.irq_pin_b  = 32 + 125
 *   Property.irq_pin_c  = 32 + 124
 *   Property.irq_pin_d  = 32 + 123
 *   Property.lanes          = 1
 *   Property.max_link_speed = 3
 * end)
 * \endcode
 *
 * The reference clock source is a board decision. This driver configures the
 * PHY to take the reference clock from the pad, which is what a board with an
 * external PCIe clock generator needs. Boards feeding the PHY from the
 * internal HSIO PLL need the PLL set up in the HSIO blk-ctrl instead.
 */

#include <l4/util/util.h>

#include "dwc_pcie_core.h"
#include "gpio"
#include <l4/drivers/hw_mmio_register_block>
#include <l4/re/env>
#include <l4/sys/arm_smccc>
#include <l4/sys/arm_smccc.h>

#include <cstdio>

namespace {

class Pci_irq_router_rs : public Resource_space
{
public:
  char const *res_type_name() const override
  { return "i.MX8M Plus PCIe IRQ router"; }

  bool request(Resource *parent, ::Device *, Resource *child,
               ::Device *cdev) override;
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

class Pcie_imx8mp : public Dwc_pcie
{
  /// Default base addresses of the blocks this driver needs.
  enum Block_defaults
  {
    Ccm_base_default  = 0x30380000,
    Gpr_base_default  = 0x30340000,
    Src_base_default  = 0x30390000,
    Phy_base_default  = 0x32f00000,
    Hsio_base_default = 0x32f10000,
  };

  L4drivers::Register_block<32> _ccm;
  L4drivers::Register_block<32> _src;
  L4drivers::Register_block<32> _gpr;
  L4drivers::Register_block<32> _hsio;
  L4drivers::Register_block<32> _phy;

  /**
   * CCM clock gates and clock roots.
   *
   * A clock gate register holds four 2-bit domain fields, the A53 cluster uses
   * domain 0. A clock root register holds the enable bit, the parent mux and
   * the two dividers, where a divider field holds the divisor minus one.
   */
  enum Ccm_regs
  {
    Ccgr_pcie     = 0x4250, ///< CCGR37: PCIE_ROOT
    Ccgr_usb      = 0x44d0, ///< CCGR79: USB_ROOT
    Ccgr_hsio     = 0x45c0, ///< CCGR92: HSIO_ROOT
    Root_hsio_axi = 0x8380, ///< clock root 7
    Root_pcie_aux = 0xa400, ///< clock root 72
  };

  enum Ccm_values
  {
    Ccgr_domain0_mask = 0x3,
    Ccgr_domain0_on   = 0x3,

    Root_enable       = 1U << 28,
    Root_mux_shift    = 24,

    /// 500 MHz: sys_pll2_500m, both dividers 1
    Root_hsio_axi_val = Root_enable | (1U << Root_mux_shift),
    /// 10 MHz: sys_pll2_50m divided by 5
    Root_pcie_aux_sel = (2U << Root_mux_shift) | 4U,
    Root_pcie_aux_val = Root_enable | Root_pcie_aux_sel,
  };

  /**
   * Firmware interface for the GPC power domains.
   *
   * The PGC block window of the GPC is not accessible from the normal world,
   * so the domains are switched through the SiP service of the firmware. It
   * runs the whole sequence: the HSIO clocks, the PGC control bit, the power
   * up request and the ADB400 handshake.
   */
  enum Sip
  {
    Sip_gpc           = 0xc2000000,
    Sip_gpc_pm_domain = 0x03,
  };

  /// Power domain ids as defined by the firmware.
  enum Sip_domain
  {
    Sip_hsiomix  = 0,
    Sip_pcie_phy = 1,
  };

  /// HSIO blk-ctrl. The reset bits release the PHY when set.
  enum Hsio_regs
  {
    Hsio_gpr_reg0 = 0x00,
  };

  enum Hsio_values
  {
    Pcie_clock_module_en = 1U << 0,
    Usb_clock_module_en  = 1U << 1,
    Pcie_phy_apb_rst     = 1U << 4,
    Pcie_phy_init_rst    = 1U << 5,
  };

  /**
   * SRC reset signals, all in one register.
   *
   * PERST and APPS_EN are active low: clearing the bit asserts the reset.
   */
  enum Src_regs
  {
    Pciephy_rcr = 0x2c,
  };

  enum Src_values
  {
    Rst_pciephy      = 1U << 2,
    Rst_pciephy_perst = 1U << 3,
    Rst_pcie_apps_en = 1U << 6,
    Rst_pcie_turnoff = 1U << 11,
  };

  /// IOMUXC_GPR
  enum Gpr_regs
  {
    Gpr12 = 0x30,
    Gpr14 = 0x38,
  };

  enum Gpr12_values
  {
    Device_type_mask = 0xfU << 12,
    Device_type_rc   = 0x4U << 12,
  };

  enum Gpr14_values
  {
    Aux_en_override     = 1U << 9,
    Clk_req_override_en = 1U << 10,
    Clk_req_override    = 1U << 11,
    Ssc_en              = 1U << 16,
    Power_off           = 1U << 17,
    Cmn_rst             = 1U << 18,
    Aux_en              = 1U << 19,
    Ref_clk_sel_mask    = 0x3U << 24,
    Ref_clk_ext         = 0x2U << 24,
  };

  /// Offsets inside the PCIe capability of the root complex.
  enum Pcie_cap_regs
  {
    Lnk_cap = 0x0c,
    Lnk_sts = 0x10,
  };

  enum Lnk_values
  {
    Lnk_speed_mask = 0xf,
    Lnk_speed_gen1 = 1,
  };

  /// LTSSM states reported in the low bits of the Debug0 register.
  enum Ltssm_state
  {
    Ltssm_state_mask      = 0x3f,
    Ltssm_detect_quiet    = 0x00,
    Ltssm_detect_act      = 0x01,
    Ltssm_poll_active     = 0x02,
    Ltssm_poll_compliance = 0x03,
  };

  /// PCIe PHY
  enum Phy_regs
  {
    Cmn_reg061 = 0x184,
    Cmn_reg075 = 0x1d4,
  };

  enum Phy_values
  {
    Ana_pll_clk_out_to_ext_io_en = 1U << 0,
    Ana_pll_done                 = 0x3,
  };

  L4::Cap<L4::Arm_smccc> _smc;

  Device_property<Hw::Gpio_device> _perst_gpio;
  Int_property _perst_pin{~0};

  Device_property<Hw::Gpio_device> _refclk_gpio;
  Int_property _refclk_pin{~0};
  Int_property _refclk_level{1};

  Int_property _ccm_base{Ccm_base_default};
  Int_property _src_base{Src_base_default};
  Int_property _gpr_base{Gpr_base_default};
  Int_property _hsio_base{Hsio_base_default};
  Int_property _phy_base{Phy_base_default};

  Int_property _int_map[4];

public:
  explicit Pcie_imx8mp(int segment = 0, unsigned bus_nr = 0);

  void init() override;
  bool controller_host_init() override;

  int int_map(int const i) const { return _int_map[i]; }

private:
  bool map_blocks();
  bool power_up_domain(unsigned const domain, char const *name);
  void enable_clocks();
  bool power_up_domains();
  void init_phy();
  bool wait_pll_lock();
  void release_board_perst();
  void wait_link_up();
};

bool Pci_irq_router_rs::request(Resource *parent, ::Device *pdev,
                                Resource *child, ::Device *cdev)
{
  auto *cd = dynamic_cast<Hw::Device *>(cdev);
  if (!cd)
    return false;

  unsigned pin = child->start();
  if (pin > 3)
    return false;

  auto *pd = dynamic_cast<Pcie_imx8mp *>(pdev);
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

Pcie_imx8mp::Pcie_imx8mp(int segment, unsigned bus_nr)
: Dwc_pcie(segment, bus_nr, this, nullptr)
{
  set_name_if_empty("pcie_imx8mp");

  register_property("perst_gpio", &_perst_gpio); // mandatory
  register_property("perst_pin", &_perst_pin);   // mandatory
  register_property("refclk_gpio", &_refclk_gpio);   // optional
  register_property("refclk_pin", &_refclk_pin);     // optional
  register_property("refclk_level", &_refclk_level); // optional

  register_property("ccm_base", &_ccm_base);     // optional
  register_property("src_base", &_src_base);     // optional
  register_property("gpr_base", &_gpr_base);     // optional
  register_property("hsio_base", &_hsio_base);   // optional
  register_property("phy_base", &_phy_base);     // optional

  register_property("irq_pin_a", &_int_map[0]);  // optional
  register_property("irq_pin_b", &_int_map[1]);  // optional
  register_property("irq_pin_c", &_int_map[2]);  // optional
  register_property("irq_pin_d", &_int_map[3]);  // optional
}

bool Pcie_imx8mp::map_blocks()
{
  struct
  {
    char const *name;
    l4_addr_t base;
    l4_size_t size;
    L4drivers::Register_block<32> *block;
  } const blocks[] =
  {
    { "ccm",  (l4_addr_t)_ccm_base.val(),  0x10000, &_ccm  },
    { "src",  (l4_addr_t)_src_base.val(),  0x10000, &_src  },
    { "gpr",  (l4_addr_t)_gpr_base.val(),  0x10000, &_gpr  },
    { "hsio", (l4_addr_t)_hsio_base.val(), 0x01000, &_hsio },
    { "phy",  (l4_addr_t)_phy_base.val(),  0x10000, &_phy  },
  };

  for (auto const &b : blocks)
    {
      l4_addr_t va = res_map_iomem(b.base, b.size);
      if (!va)
        {
          error("could not map '%s' memory at %08lx.", b.name, b.base);
          return false;
        }
      *b.block = new L4drivers::Mmio_register_block<32>(va);
    }

  return true;
}

void Pcie_imx8mp::enable_clocks()
{
  /* The clock roots carry CLK_SET_PARENT_GATE semantics: select the parent and
     the dividers while the root is still gated, then enable it. */
  _ccm[Root_hsio_axi] = Root_hsio_axi_val & ~Root_enable;
  _ccm[Root_hsio_axi] = Root_hsio_axi_val;

  _ccm[Root_pcie_aux] = Root_pcie_aux_sel;
  _ccm[Root_pcie_aux] = Root_pcie_aux_val;

  _ccm[Ccgr_hsio].modify(Ccgr_domain0_mask, Ccgr_domain0_on);
  _ccm[Ccgr_pcie].modify(Ccgr_domain0_mask, Ccgr_domain0_on);
  _ccm[Ccgr_usb].modify(Ccgr_domain0_mask, Ccgr_domain0_on);
}

bool Pcie_imx8mp::power_up_domain(unsigned const domain, char const *name)
{
  l4_umword_t out0, out1, out2, out3;

  l4_msgtag_t tag = l4_arm_smccc_call(_smc.cap(), Sip_gpc, Sip_gpc_pm_domain,
                                      domain, 1, 0, 0, 0,
                                      &out0, &out1, &out2, &out3, 0);
  int err = l4_error(tag);
  if (err < 0)
    {
      error("firmware call to power up the %s domain failed: %d.", name, err);
      return false;
    }

  return true;
}

bool Pcie_imx8mp::power_up_domains()
{
  if (!power_up_domain(Sip_hsiomix, "HSIOMIX"))
    return false;

  // PCIe controller clocks inside the HSIO block
  _hsio[Hsio_gpr_reg0].set(Pcie_clock_module_en);

  // Release the PHY APB and init resets before powering the PHY domain.
  _hsio[Hsio_gpr_reg0].set(Pcie_phy_apb_rst | Pcie_phy_init_rst);

  if (!power_up_domain(Sip_pcie_phy, "PCIE_PHY"))
    return false;

  return true;
}

void Pcie_imx8mp::init_phy()
{
  // Hold the PHY and the controller in reset while the PHY is configured.
  _src[Pciephy_rcr].set(Rst_pciephy);
  _src[Pciephy_rcr].clear(Rst_pciephy_perst | Rst_pcie_apps_en);

  // CLKREQ# is not wired up, so drive the override instead.
  _gpr[Gpr14].clear(Clk_req_override);
  _gpr[Gpr14].set(Clk_req_override_en);

  // Take the reference clock from the pad.
  _phy[Cmn_reg061].clear(Ana_pll_clk_out_to_ext_io_en);

  _gpr[Gpr14].clear(Aux_en_override);
  _gpr[Gpr14].set(Aux_en);
  _gpr[Gpr14].clear(Power_off);
  _gpr[Gpr14].clear(Ssc_en);
  _gpr[Gpr14].modify(Ref_clk_sel_mask, Ref_clk_ext);
  l4_usleep(100);

  _src[Pciephy_rcr].set(Rst_pciephy_perst);
  _src[Pciephy_rcr].clear(Rst_pciephy);
  l4_usleep(200);

  // The PHY common block reset has to follow the PHY reset release.
  _gpr[Gpr14].set(Cmn_rst);
}

bool Pcie_imx8mp::wait_pll_lock()
{
  for (unsigned retries = 0; retries < 2000; ++retries)
    {
      if (_phy[Cmn_reg075] == Ana_pll_done)
        {
          d_printf(DBG_INFO, "%s: PCIe PHY PLL locked\n", name());
          return true;
        }
      l4_usleep(10);
    }

  error("PCIe PHY PLL lock timeout, check the reference clock generator.");
  return false;
}

void Pcie_imx8mp::release_board_perst()
{
  auto *gpio = dynamic_cast<Hw::Gpio_chip *>(_perst_gpio.dev());

  /* PERST# is active low, so driving the pin high releases the card. The low
     level is driven first to generate the reset pulse. */
  gpio->setup(_perst_pin, Hw::Gpio_chip::Fix_mode::Output, 0);
  gpio->set(_perst_pin, 0);
  l4_sleep(10);
  gpio->set(_perst_pin, 1);

  // PCIe r5.0, 6.6.1 asks for 100ms after PERST# de-assertion.
  l4_sleep(100);
}

void Pcie_imx8mp::wait_link_up()
{
  for (unsigned retries = 0; retries < 10; ++retries)
    {
      if (link_up())
        return;
      l4_usleep(90000);
    }
}

bool Pcie_imx8mp::controller_host_init()
{
  if (_perst_gpio.dev() == nullptr)
    {
      error("'perst_gpio' not set.");
      return false;
    }

  if (_perst_pin == ~0)
    {
      error("'perst_pin' not set.");
      return false;
    }

  _smc = L4Re::Env::env()->get_cap<L4::Arm_smccc>("arm_smc");
  if (!_smc.is_valid())
    {
      error("no 'arm_smc' capability, cannot switch the power domains.");
      return false;
    }

  if (!map_blocks())
    return false;

  /* Drive the output enable of an external reference clock generator, if the
     board routes it to a pin. The PHY needs the clock to lock its PLL, so
     this has to happen before the PHY is brought up. */
  if (_refclk_gpio.dev() != nullptr && _refclk_pin != ~0)
    {
      auto *gpio = dynamic_cast<Hw::Gpio_chip *>(_refclk_gpio.dev());

      gpio->setup(_refclk_pin, Hw::Gpio_chip::Fix_mode::Output, 0);
      gpio->set(_refclk_pin, _refclk_level ? 1 : 0);
      d_printf(DBG_INFO, "%s: reference clock output enable driven %s\n",
               name(), _refclk_level ? "high" : "low");
      l4_sleep(10);
    }

  enable_clocks();

  if (!power_up_domains())
    return false;

  init_phy();

  if (!wait_pll_lock())
    return false;

  release_board_perst();

  // Operate the core as root complex.
  _gpr[Gpr12].modify(Device_type_mask, Device_type_rc);

  return true;
}

void Pcie_imx8mp::init()
{
  if (!Dwc_pcie::host_init())
    return;

  d_printf(DBG_INFO, "%s: iATU: %u outbound, %u inbound windows\n", name(),
           _num_ob_windows, _num_ib_windows);

  if (!setup_rc())
    return;

  /* Train the link at Gen1 first and negotiate up afterwards. The core does
     not reliably come up when training starts at the target speed. */
  l4_uint32_t target_speed = _regs[_offs_cap_pcie + Lnk_cap] & Lnk_speed_mask;
  _regs[_offs_cap_pcie + Lnk_cap].modify(Lnk_speed_mask, Lnk_speed_gen1);

  // Enabling the LTSSM starts link training.
  _src[Pciephy_rcr].set(Rst_pcie_apps_en);

  wait_link_up();

  if (link_up() && target_speed > Lnk_speed_gen1)
    {
      _regs[_offs_cap_pcie + Lnk_cap].modify(Lnk_speed_mask, target_speed);
      _regs[Port_logic::Gen2].set(1U << Pl_gen2::Speed_change_shift);
      wait_link_up();
    }

  if (!link_up())
    {
      unsigned ltssm = _regs[Port_logic::Debug0] & Ltssm_state_mask;
      char const *reason;

      /* A link that stays in detect never saw a receiver, while one that
         reaches polling found a device that does not answer with training
         sequences. */
      switch (ltssm)
        {
        case Ltssm_detect_quiet:
        case Ltssm_detect_act:
          reason = "device not found";
          break;
        case Ltssm_poll_active:
        case Ltssm_poll_compliance:
          reason = "device found, but not active";
          break;
        default:
          reason = "link training did not complete";
          break;
        }

      d_printf(DBG_WARN, "%s: Link DOWN, %s: LTSSM state 0x%x\n", name(),
               reason, ltssm);
      return;
    }

  l4_uint32_t sts = _regs[_offs_cap_pcie + Lnk_sts] >> 16;
  d_printf(DBG_INFO, "%s: Link up, gen %d, x%d\n", name(),
           (int)(sts & Lnk_speed_mask), (int)((sts >> 4) & 0x3f));

  typedef Hw::Pci::Irq_router_res<Pci_irq_router_rs> Irq_res;
  Irq_res *ir = new Irq_res();
  ir->set_id("IRQR");
  add_resource_rq(ir);

  discover_bus(this, this);
  Hw::Device::init();
}

static Hw::Device_factory_t<Pcie_imx8mp> f("Pcie_imx8mp");

}
