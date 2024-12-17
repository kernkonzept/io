-- vi:ft=lua

local Res = Io.Res
local Hw = Io.Hw

local add_children = Io.Dt.add_children

add_children(Io.system_bus(), function()
  CPUID = Hw.Device(function()
    compatible = { "samsung,exynos4210-chipid" };
    Property.hid = "exynos-cpuid";
    Resource.reg0 = Res.mmio(0x10000000, 0x10000fff);
  end)

  CLOCK = Hw.Device(function()
    Property.hid="exynos5250-clock";
    Resource.reg0 = Res.mmio(0x10010000, 0x1003ffff);
  end)

  SATA = Hw.Device(function()
    Property.hid =  "exynos5-sata-ahci";
    Resource.reg0 = Res.mmio(0x122F0000, 0x122f01ff);
    Resource.irq0 = Res.irq(147);
  end)

  SATA_PHY = Hw.Device(function()
    Property.hid =  "exynos5-sata-phy";
    Resource.reg0 = Res.mmio(0x12170000, 0x121701ff);
--  Resource.irq = Res.irq(140);
--  Resource.mmio = Res.mmio(0x10040000, 0x10040fff);
  end)

  SATA_PHY_I2C = Hw.Device(function()
    Property.hid =  "exynos5-sata-phy-i2c";
    Resource.reg0 = Res.mmio(0x121D0000, 0x121D0100);
  end)

  USB_EHCI = Hw.Device(function()
    Property.hid = "exynos4210-ehci";
    Resource.reg0 = Res.mmio(0x12110000, 0x121100ff);
    Resource.irq0 = Res.irq(103);
  end)

  USB_OHCI = Hw.Device(function()
    Property.hid = "exynos4210-ohci";
    Resource.reg0 = Res.mmio(0x12120000, 0x121200ff);
    Resource.irq0 = Res.irq(103);
  end)

  USB_PHY = Hw.Device(function()
    Property.hid = "exynos5250-usb2phy";
    Resource.reg0 = Res.mmio(0x12130000, 0x121300ff);
--  Resource.mmio = Res.mmio(0x10040704, 0x1004070b);
--  Resource.mm1 = Res.mmio(0x10050230, 0x10050233);
--  Resource.mm2 = Res.mmio(0x10050000, 0x10050fff);
  end)

  USB3 = Hw.Device(function()
    Property.hid = "exynos5-usb3";
    Resource.reg0 = Res.mmio(0x12000000, 0x1210ffff);
    Resource.irq0 = Res.irq(104);
  end)

  INT_COMB = Hw.Device(function()
    Property.hid = "exynos-comb";
    Resource.reg0 = Res.mmio(0x10440000, 0x1045ffff);
    for z = 32, 63 do
      Resource[#Resource + 1] = Res.irq(z);
    end

  end)

  PDMA = Hw.Device(function()
    Property.hid = "exynos";
    Resource.reg0 = Res.mmio(0x121a0000, 0x121bffff);
    Resource.irq0 = Res.irq(66);
    Resource.irq1 = Res.irq(67);
  end)

  MDMA0 = Hw.Device(function()
    Property.hid = "exynos";
    Resource.reg0 = Res.mmio(0x10800000, 0x1080ffff);
    Resource.irq0 = Res.irq(65);
  end)

  MDMA1 = Hw.Device(function()
    Property.hid = "exynos";
    Resource.reg0 = Res.mmio(0x11c10000, 0x11c1ffff);
    Resource.irq0 = Res.irq(156);
  end)

  ALIVE = Hw.Device(function()
    Property.hid = "exynos";
    _res = { Res.mmio(0x10040000, 0x1004ffff) }
  end)

  SYSREG = Hw.Device(function()
    Property.hid = "exynos";
    _res = { Res.mmio(0x10050000, 0x1005ffff) }
  end)

  I2Cs = Hw.Device(function()
    Property.hid = "exynos";
    Resource.reg0 = Res.mmio(0x12c60000, 0x12ceffff);
    Resource.irq0 = Res.irq(88);
    Resource.irq1 = Res.irq(89);
    Resource.irq2 = Res.irq(90);
    Resource.irq3 = Res.irq(91);
    Resource.irq4 = Res.irq(92);
    Resource.irq5 = Res.irq(93);
    Resource.irq6 = Res.irq(94);
    Resource.irq7 = Res.irq(95);
    Resource.irq8 = Res.irq(96);
  end)

  PWM = Hw.Device(function()
    Property.hid = "exynos";
    Resource.reg0 = Res.mmio(0x12dd0000, 0x12dd0fff);
  end)

  GPIO = Hw.Device(function()
    Property.hid = "exynos";
    Resource.irq0 = Res.irq(77);
    Resource.irq1 = Res.irq(78);
    Resource.irq2 = Res.irq(79);
    Resource.irq3 = Res.irq(82);
    Resource.reg0 = Res.mmio(0x10d10000, 0x10d1ffff);
    Resource.reg1 = Res.mmio(0x11400000, 0x1140ffff);
    Resource.reg2 = Res.mmio(0x13400000, 0x1340ffff);
  end)

  RTC = Hw.Device(function()
    Property.hid = "exynos-rtc";
    Resource.reg0 = Res.mmio(0x101e0000, 0x101e0fff);
    Resource.irq0 = Res.irq(75);
    Resource.irq1 = Res.irq(76);
  end)

  AUDSS = Hw.Device(function()
    Property.hid = "exynos-audss";
    Resource.reg0 = Res.mmio(0x03810000, 0x03810fff);
  end)

  UART0 = Hw.Device(function()
    Property.hid = "exynos-serial0";
    Resource.reg0 = Res.mmio(0x12C00000, 0x12C0ffff);
  end)

  UART1 = Hw.Device(function()
    Property.hid = "exynos-serial1";
    Resource.reg0 = Res.mmio(0x12C10000, 0x12C1ffff);
  end)

  UART2 = Hw.Device(function()
    Property.hid = "exynos-serial2";
    Resource.reg0 = Res.mmio(0x12C20000, 0x12C2ffff);
  end)

  UART3 = Hw.Device(function()
    Property.hid = "exynos-serial3";
    Resource.reg0 = Res.mmio(0x12C30000, 0x12C3ffff);
  end)

end)
