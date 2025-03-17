-- vi:ft=lua

local Res = Io.Res
local Hw = Io.Hw
local Vi = Io.Vi
local root = Io.system_bus();
local sb = {}

local add_children = Io.Dt.add_children

add_children(Io.system_bus(), function()
  ethernet_gmac = Hw.Device(function()
    compatible = "fsl,s32cc-dwmac";
    Property.hid = "fsl,s32cc-dwmac";
    Resource.reg0 = Res.mmio(0x4033c000, 0x4033c000 + 0x2000 - 1);
    Resource.reg1 = Res.mmio(0x4007c004, 0x4007c007);
    Resource.irq0 = Res.irq(32 + 57, Io.Resource.Irq_type_level_high);
  end)

  ethernet_syscon = Hw.Device(function()
    compatible = "fsl,s32-xxx1";
    Property.hid = "fsl,s32-xxx1";
    Resource.reg0 = Res.mmio(0x4007C000, 0x4007C000 + 0x1000 - 1);
  end)

  clks  = Hw.Device(function()
    compatible = "fsl,s32g275-clocking";
    Property.hid = "fsl,s32g275-clocking";
    Resource.reg0 = Res.mmio(0x40038000, 0x40038000 + 0x3000 - 1); -- armpll
    Resource.reg1 = Res.mmio(0x4003C000, 0x4003C000 + 0x3000 - 1); -- periphpll
    Resource.reg2 = Res.mmio(0x40040000, 0x40040000 + 0x3000 - 1); -- accelpll
    Resource.reg3 = Res.mmio(0x40044000, 0x40044000 + 0x3000 - 1); -- ddrpll
    Resource.reg4 = Res.mmio(0x40054000, 0x40054000 + 0x3000 - 1); -- armdfs
    Resource.reg5 = Res.mmio(0x40058000, 0x40058000 + 0x3000 - 1); -- periphdfs
  end)

  mc_cgm0 = Hw.Device(function()
    compatible = "fsl,s32gen1-mc_cgm0";
    Property.hid = "fsl,s32gen1-mc_cgm0";
    Resource.reg0 = Res.mmio(0x40030000, 0x40030000 + 0x3000 - 1);
  end)

  mc_cgm1 = Hw.Device(function()
    compatible = "fsl,s32gen1-mc_cgm1";
    Property.hid = "fsl,s32gen1-mc_cgm1";
    Resource.reg0 = Res.mmio(0x40034000, 0x40034000 + 0x3000 - 1);
  end)

  mc_cgm2 = Hw.Device(function()
    compatible = "fsl,s32gen1-mc_cgm2";
    Property.hid = "fsl,s32gen1-mc_cgm2";
    Resource.reg0 = Res.mmio(0x44018000, 0x44018000 + 0x3000 - 1);
  end)

  mc_cgm5 = Hw.Device(function()
    compatible = "fsl,s32gen1-mc_cgm5";
    Property.hid = "fsl,s32gen1-mc_cgm5";
    Resource.reg0 = Res.mmio(0x40068000, 0x40068000 + 0x3000 - 1);
  end)

  usdhc0 = Hw.Device(function()
    compatible = "fsl,s32gen1-usdhc";
    Property.hid = "fsl,s32gen1-usdhc";
    Property.flags = Io.Hw_device_DF_dma_supported;
    Resource.reg0 = Res.mmio(0x402F0000, 0x402F0fff);
    Resource.irq0 = Res.irq(32 + 36, Io.Resource.Irq_type_level_high);
  end)

  rtc = Hw.Device(function()
    compatible = "fsl,s32gen1-rtc";
    Property.hid = "fsl,s32gen1-rtc";
    Resource.reg0 = Res.mmio(0x40060000, 0x40060fff);
    Resource.irq0 = Res.irq(32 + 121, Io.Resource.Irq_type_level_high);
  end)

  mu0 = Hw.Device(function()
    compatible = "fsl,s32gen1-hse";
    Property.hid = "fsl,s32gen1-hse";
    Resource.reg0 = Res.mmio(0x40210000, 0x40210000 + 0x1000 - 1);
    Resource.irq0 = Res.irq(32 + 103, Io.Resource.Irq_type_raising_edge);
    Resource.irq1 = Res.irq(32 + 104, Io.Resource.Irq_type_raising_edge);
    Resource.irq2 = Res.irq(32 + 105, Io.Resource.Irq_type_raising_edge);
  end)

  mu1 = Hw.Device(function()
    compatible = "fsl,s32gen1-hse";
    Property.hid = "fsl,s32gen1-hse";
    Resource.reg0 = Res.mmio(0x40220000, 0x40220000 + 0x1000 - 1);
    Resource.irq0 = Res.irq(32 + 106, Io.Resource.Irq_type_raising_edge);
    Resource.irq1 = Res.irq(32 + 107, Io.Resource.Irq_type_raising_edge);
    Resource.irq2 = Res.irq(32 + 108, Io.Resource.Irq_type_raising_edge);
  end)

  mu2 = Hw.Device(function()
    compatible = "fsl,s32gen1-hse";
    Property.hid = "fsl,s32gen1-hse";
    Resource.reg0 = Res.mmio(0x40230000, 0x40230000 + 0x1000 - 1);
    Resource.irq0 = Res.irq(32 + 109, Io.Resource.Irq_type_raising_edge);
    Resource.irq1 = Res.irq(32 + 110, Io.Resource.Irq_type_raising_edge);
    Resource.irq2 = Res.irq(32 + 111, Io.Resource.Irq_type_raising_edge);
  end)

  mu3 = Hw.Device(function()
    compatible = "fsl,s32gen1-hse";
    Property.hid = "fsl,s32gen1-hse";
    Resource.reg0 = Res.mmio(0x40240000, 0x40240000 + 0x1000 - 1);
    Resource.irq0 = Res.irq(32 + 112, Io.Resource.Irq_type_raising_edge);
    Resource.irq1 = Res.irq(32 + 113, Io.Resource.Irq_type_raising_edge);
    Resource.irq2 = Res.irq(32 + 114, Io.Resource.Irq_type_raising_edge);
  end)
end)
