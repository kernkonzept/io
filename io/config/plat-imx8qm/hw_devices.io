-- vim:set ft=lua:
--
-- SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom
-- Copyright (C) 2023 Kernkonzept GmbH.
-- Author(s): Christian Pötzsch <christian.poetzsch@kernkonzept.com>
--            Frank Mehnert <frank.mehnert@kernkonzept.com>
--
-- NXP/Freescale i.MX8

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()

  scu_5d1c0000 = Io.Hw.Scu_imx8qm(function()
    compatible    = { "fsl,imx8qm-mu", "fsl,imx6sx-mu" }
    Resource.regs = reg_mmio(0x5d1c0000, 0x00010000)
    Resource.irq0 = Io.Res.irq(32 + 177, Io.Resource.Irq_type_level_high)
  end)

  gpio_5d0b0000 = Io.Hw.Gpio_imx8qm_chip(function()
    compatible     = { "fsl,imx8qm-gpio", "l4vmm,imx8qm-gpio" }
    Property.hid   = "l4vmm,imx8qm-gpio"
    Property.scu   = scu_5d1c0000
    Property.power = 202 -- IMX_SC_R_GPIO_3
    Resource.regs  = reg_mmio(0x5d0b0000, 0x00001000)
    Resource.irq0  = Io.Res.irq(32 + 139, Io.Resource.Irq_type_level_high)
  end)

  gpio_5d0c0000 = Io.Hw.Gpio_imx8qm_chip(function()
    compatible     = { "fsl,imx8qm-gpio", "l4vmm,imx8qm-gpio" }
    Property.hid   = "l4vmm,imx8qm-gpio"
    Property.scu   = scu_5d1c0000
    Property.power = 203 -- IMX_SC_R_GPIO_4
    Resource.regs  = reg_mmio(0x5d0c0000, 0x00001000)
    Resource.irq0  = Io.Res.irq(32 + 140, Io.Resource.Irq_type_level_high)
  end)

  scu   = scu_5d1c0000
  gpio3 = gpio_5d0b0000
  gpio4 = gpio_5d0c0000

end)


Io.Dt.add_children(Io.system_bus(), function()

  bus_30000000 = Hw.Device(function() -- AIPS1
    compatible = { "fsl,aips-bus", "simple-bus" }
    Property.hid = "fsl,aips-bus"
    Resource.req0 = Io.Res.mmio(0x30000000, 0x303fffff)
  end)

  bus_30400000 = Hw.Device(function() -- AIPS2
    compatible = { "fsl,aips-bus", "simple-bus" }
    Property.hid = "fsl,aips-bus"
    Resource.req0 = Io.Res.mmio(0x30400000, 0x307fffff)
  end)

  bus_30800000 = Hw.Device(function() -- AIPS3
    compatible = { "fsl,aips-bus", "simple-bus" }
    Property.hid = "fsl,aips-bus"
    Resource.req0 = Io.Res.mmio(0x30800000, 0x30bfffff)
  end)

  bus_32c00000 = Hw.Device(function() -- AIPS4
    compatible = { "fsl,aips-bus", "simple-bus" }
    Property.hid = "fsl,aips-bus"
    Resource.req0 = Io.Res.mmio(0x32c00000, 0x32ffffff)
  end)

  clock_controller_30380000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-ccm" }
    Property.hid = "fsl,imx8mq-ccm"
    Resource.req0 = Io.Res.mmio(0x30380000, 0x3038ffff)
    Resource.irq0 = Io.Res.irq(32 + 85, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 86, Io.Resource.Irq_type_level_high)
  end)

  gpc_303a0000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-gpc" }
    Property.hid = "fsl,imx8mq-gpc"
    Resource.req0 = Io.Res.mmio(0x303a0000, 0x303affff)
  end)

  timer_306a0000 = Hw.Device(function()
    compatible = { "nxp,sysctr-timer" }
    Property.hid = "nxp,sysctr-timer"
    Resource.req0 = Io.Res.mmio(0x306a0000, 0x306bffff)
    Resource.irq0 = Io.Res.irq(32 + 47, Io.Resource.Irq_type_level_high)
  end)

  efuse_30350000 = Hw.Device(function() -- ocotp
    compatible = { "fsl,imx8mq-ocotp", "syscon" }
    Property.hid = "fsl,imx8mq-ocotp"
    Resource.req0 = Io.Res.mmio(0x30350000, 0x3035ffff)
  end)

  syscon_30360000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-anatop", "syscon" }
    Property.hid = "fsl,imx8mq-anatop"
    Resource.req0 = Io.Res.mmio(0x30360000, 0x3036ffff)
  end)

  mmc_30b40000 = Hw.Device(function() -- usdhc1
    compatible = { "fsl,imx8mq-usdhc", "fsl,imx7d-usdhc" }
    Property.hid = "fsl,imx8mq-usdhc"
    Resource.req0 = Io.Res.mmio(0x30b40000, 0x30b4ffff)
    Resource.irq0 = Io.Res.irq(32 + 22, Io.Resource.Irq_type_level_high)
  end)

  mmc_30b50000 = Hw.Device(function() -- usdhc2
    compatible = { "fsl,imx8mq-usdhc", "fsl,imx7d-usdhc" }
    Property.hid = "fsl,imx8mq-usdhc"
    Resource.req0 = Io.Res.mmio(0x30b50000, 0x30b5ffff)
    Resource.irq0 = Io.Res.irq(32 + 23, Io.Resource.Irq_type_level_high)
  end)

  gpio_30200000 = Hw.Device(function() -- gpio1
    compatible = { "fsl,imx8mq-gpio", "fsl,imx35-gpio" }
    Property.hid = "fsl,imx8mq-gpio"
    Resource.req0 = Io.Res.mmio(0x30200000, 0x3020ffff)
    Resource.irq0 = Io.Res.irq(32 + 64, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 65, Io.Resource.Irq_type_level_high)
  end)

  gpio_30210000 = Hw.Device(function() -- gpio2
    compatible = { "fsl,imx8mq-gpio", "fsl,imx35-gpio" }
    Property.hid = "fsl,imx8mq-gpio"
    Resource.req0 = Io.Res.mmio(0x30210000, 0x3021ffff)
    Resource.irq0 = Io.Res.irq(32 + 66, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 67, Io.Resource.Irq_type_level_high)
  end)

  gpio_30220000 = Hw.Device(function() -- gpio3
    compatible = { "fsl,imx8mq-gpio", "fsl,imx35-gpio" }
    Property.hid = "fsl,imx8mq-gpio"
    Resource.req0 = Io.Res.mmio(0x30220000, 0x3022ffff)
    Resource.irq0 = Io.Res.irq(32 + 68, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 69, Io.Resource.Irq_type_level_high)
  end)

  gpio_30230000 = Hw.Device(function() -- gpio4
    compatible = { "fsl,imx8mq-gpio", "fsl,imx35-gpio" }
    Property.hid = "fsl,imx8mq-gpio"
    Resource.req0 = Io.Res.mmio(0x30230000, 0x3023ffff)
    Resource.irq0 = Io.Res.irq(32 + 70, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 71, Io.Resource.Irq_type_level_high)
  end)

  gpio_30240000 = Hw.Device(function() -- gpio5
    compatible = { "fsl,imx8mq-gpio", "fsl,imx35-gpio" }
    Property.hid = "fsl,imx8mq-gpio"
    Resource.req0 = Io.Res.mmio(0x30240000, 0x3024ffff)
    Resource.irq0 = Io.Res.irq(32 + 72, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 73, Io.Resource.Irq_type_level_high)
  end)

  spi_30bb0000 = Hw.Device(function() -- qspi0
    compatible = { "fsl,imx8mq-qspi", "fsl,imx7d-qspi" }
    Property.hid = "fsl,imx8mq-qspi"
    Resource.req0 = Io.Res.mmio(0x30bb0000, 0x30bbffff)
    Resource.req1 = Io.Res.mmio(0x08000000, 0x17ffffff)
    Resource.irq0 = Io.Res.irq(32 + 107, Io.Resource.Irq_type_level_high)
  end)

  ethernet_30be0000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-fec", "fsl,imx6sx-fec" }
    Property.hid = "fsl,imx8mq-fec"
    Resource.req0 = Io.Res.mmio(0x30be0000, 0x30beffff)
    Resource.irq0 = Io.Res.irq(32 + 118, Io.Resource.Irq_type_level_high)
    Resource.irq0 = Io.Res.irq(32 + 119, Io.Resource.Irq_type_level_high)
    Resource.irq0 = Io.Res.irq(32 + 120, Io.Resource.Irq_type_level_high)
  end)

  ddr_pmu_3d800000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-ddr-pmu", "fsl,imx8m-ddr-pm" }
    Property.hid = "fsl,imx8mq-ddr-pmu"
    Resource.req0 = Io.Res.mmio(0x3d800000, 0x3dbfffff)
    Resource.irq0 = Io.Res.irq(32 + 98, Io.Resource.Irq_type_level_high)
  end)

  pinctrl_30330000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-iomuxc" }
    Property.hid = "fsl,imx8mq-iomuxc"
    Resource.req0 = Io.Res.mmio(0x30330000, 0x3033ffff)
  end)

  pwm_30660000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-pwm", "fsl,imx27-pwm" }
    Property.hid = "fsl,imx8mq-pwm"
    Resource.req0 = Io.Res.mmio(0x30660000, 0x3066ffff)
    Resource.irq0 = Io.Res.irq(32 + 81, Io.Resource.Irq_type_level_high)
  end)

  pwm_30670000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-pwm", "fsl,imx27-pwm" }
    Property.hid = "fsl,imx8mq-pwm"
    Resource.req0 = Io.Res.mmio(0x30670000, 0x3067ffff)
    Resource.irq0 = Io.Res.irq(32 + 82, Io.Resource.Irq_type_level_high)
  end)

  pwm_30680000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-pwm", "fsl,imx27-pwm" }
    Property.hid = "fsl,imx8mq-pwm"
    Resource.req0 = Io.Res.mmio(0x30680000, 0x3068ffff)
    Resource.irq0 = Io.Res.irq(32 + 83, Io.Resource.Irq_type_level_high)
  end)

  pwm_30690000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-pwm", "fsl,imx27-pwm" }
    Property.hid = "fsl,imx8mq-pwm"
    Resource.req0 = Io.Res.mmio(0x30690000, 0x3069ffff)
    Resource.irq0 = Io.Res.irq(32 + 84, Io.Resource.Irq_type_level_high)
  end)

  reset_controller_30390000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-src", "syscon" }
    Property.hid = "fsl,imx8mq-src", "syscon"
    Resource.req0 = Io.Res.mmio(0x30390000, 0x3039ffff)
    Resource.irq0 = Io.Res.irq(32 + 89, Io.Resource.Irq_type_level_high)
  end)

  i2c_30a20000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-i2c", "fsl,imx21-i2c" }
    Property.hid = "fsl,imx8mq-i2c"
    Resource.req0 = Io.Res.mmio(0x30a20000, 0x30a2ffff)
    Resource.irq0 = Io.Res.irq(32 + 35, Io.Resource.Irq_type_level_high)
  end)

  irqsteer_32e2d000 = Hw.Device(function()
    compatible = { "fsl,imx8m-irqsteer", "fsl,imx-irqsteer" }
    Property.hid = "fsl,imx8m-irqsteer"
    Resource.req0 = Io.Res.mmio(0x32e2d000, 0x32e2dfff)
    Resource.irq0 = Io.Res.irq(32 + 18, Io.Resource.Irq_type_level_high)
  end)

  syscon_3034000 = Hw.Device(function()
    compatible = { "fsl,imx8mq-iomuxc-gpr", "fsl,imx6q-iomuxc-gpr",
                   "syscon", "simple-mfd" }
    Property.hid = "fsl,imx8mq-iomuxc-gpr"
    Resource.req = Io.Res.mmio(0x30340000, 0x3034ffff)
  end)

  -- IO drives the PCIe controller and provides the attached PCIe devices to
  -- clients.
  --[[
  pciec0 = Io.Hw.Pcie_imx8qm(function()
    Property.scu       = scu
    Property.gpio3     = gpio3
    Property.gpio4     = gpio4
    Property.power     =
            {
              152, -- IMX_SC_R_PCIE_A (pciea and HSIO crr)
              153, -- IMX_SC_R_SERDES_0 (pciea and HSIO crr)
              172, -- IMX_SC_R_HSIO_GPIO (pciea and HSIO crr)
              169, -- IMX_SC_R_PCIE_B (pcieb and HSIO crr)
              171, -- IMX_SC_R_SERDES_1 (pcieb and HSIO crr)
            }
    Property.pads      =
            {
              129, 3, 0x00000021, -- IMX8QM_SAI1_RXFS_LSIO_GPIO3_IO14
            }
    Property.clks      = {}         -- no clocks need to be enabled
    Property.regs_base = 0x5f000000 -- reg: dbi (verified by pci-imx6.c)
    Property.regs_size = 0x00010000
    Property.cfg_base  = 0x6ff00000 -- reg: config
    Property.cfg_size  = 0x00080000
    -- ignore the 1st ranges statement in pciea because that's port I/O
    Property.mem_base  = 0x60000000 -- downstream non-prefetchable 32-bit MMIO
    Property.mem_size  = 0x0ff00000
    Property.cpu_fixup = 0x40000000 -- see 'local-addr'
    Property.irq_pin_a = 32 + 73
    Property.irq_pin_b = 32 + 74
    Property.irq_pin_c = 32 + 75
    Property.irq_pin_d = 32 + 76
    Property.lanes     = 1
  end)
  --]]

  -- IO makes the PCIe controller available to a client. The client must know
  -- how to drive the controller.
  --[[
  pcie_5f000000 = Hw.Device(function()
    compatible = { "fsl,imx8qm-pcie" }
    Property.hid= "fsl,imx8qm-pcie"
    Resource.req0 = Io.Res.mmio(0x5f000000, 0x5f00ffff) -- controller reg
    Resource.req1 = Io.Res.mmio(0x6ff00000, 0x6ff7ffff) -- config space
    Resource.irq0 = Io.Res.irq(32 + 73, Io.Resource.Irq_type_level_high)
    Resource.irq1 = Io.Res.irq(32 + 74, Io.Resource.Irq_type_level_high)
    Resource.irq2 = Io.Res.irq(32 + 75, Io.Resource.Irq_type_level_high)
    Resource.irq3 = Io.Res.irq(32 + 76, Io.Resource.Irq_type_level_high)
  end)
  --]]

end)
