-- vi:ft=lua

local Res = Io.Res
local Hw = Io.Hw

Io.hw_add_devices(function()

  GPIO = Hw.Gpio_bcm2835_chip(function()
    compatible = {"brcm,bcm2835-gpio"};
    Property.hid = "gpio-bcm2835-GPIO";
    Resource.regs = Res.mmio(0xfe200000, 0xfe2000b4 - 1);
    Property.pins = 58;
    Resource.int0 = Res.irq(145, Io.Resource.Irq_type_level_high)
    Resource.int2 = Res.irq(147, Io.Resource.Irq_type_level_high)
  end);

  MBOX = Hw.Device(function()
    compatible = {"brcm,bcm2835-mbox"};
    Property.hid = "BCM2835_mbox";
    Property.flags = Io.Hw_device_DF_dma_supported;
    Resource.regs = Res.mmio(0xfe00b880, 0xfe00b8bf);
    Resource.irq0 = Res.irq(32 + 0x21, Io.Resource.Irq_type_level_high);
  end);

  FB = Hw.Device(function()
    Property.hid = "BCM2835_fb";
    Resource.mem = Res.mmio(0x5c006000, 0x60005fff);
  end);

  i2c1 = Hw.Device(function()
    compatible = {"brcm,bcm2835-i2c"};
    Property.hid = "BCM2835_bsc1";
    Resource.regs = Res.mmio(0xfe804000, 0xfe804fff);
    Resource.irq = Res.irq(32 + 117, Io.Resource.Irq_type_level_high);
    Resource.pins = Io.Gpio_resource(GPIO, 2, 3)
  end);

  cprman = Hw.Device(function()
    compatible = {"brcm,bcm2835-cprman"};
    Resource.regs = Res.mmio(0xfe101000, 0xfe103fff);
  end);

  EMMC = Hw.Device(function()
    compatible = {"brcm,bcm2711-emmc2"};
    Property.hid = "EMMC-bcm2711-emmc2";
    Property.flags = Io.Hw_device_DF_dma_supported;
    -- The device lives at the local address 0x7e340000, but the bus translates
    -- that range.
    Resource.regs = Res.mmio(0xfe340000, 0xfe340000 + 0x100 - 1);

    Resource.irq = Res.irq(32 + 126, Io.Resource.Irq_type_level_high);
  end);

  SPI = Hw.Device(function()
    compatible = {"brcm,brcm2835-spi"};
    Property.hid = "BCM2835_spi";
    Resource.regs = Res.mmio(0xFE204000, 0xFE204000 + 0x200 - 1);
    Resource.irq0 = Res.irq(32 + 118, Io.Resource.Irq_type_level_high);
    Resource.pins = Io.Gpio_resource(GPIO, 7, 11);
  end);

  dma_7e007000 = Io.Hw.Device(function()
    compatible    = { "brcm,bcm2835-dma" };
    --                      [0x7e007000, 0x00000b00]
    Property.flags = Io.Hw_device_DF_dma_supported;
    Resource.reg0 = Res.mmio(0xfe007000, 0xfe007b00 - 1);
    Resource.irq0 = Res.irq(32 + 0x50, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32 + 0x51, Io.Resource.Irq_type_level_high);
    Resource.irq2 = Res.irq(32 + 0x52, Io.Resource.Irq_type_level_high);
    Resource.irq3 = Res.irq(32 + 0x53, Io.Resource.Irq_type_level_high);
    Resource.irq4 = Res.irq(32 + 0x54, Io.Resource.Irq_type_level_high);
    Resource.irq5 = Res.irq(32 + 0x55, Io.Resource.Irq_type_level_high);
    Resource.irq6 = Res.irq(32 + 0x56, Io.Resource.Irq_type_level_high);
    Resource.irq7 = Res.irq(32 + 0x57, Io.Resource.Irq_type_level_high);
    Resource.irq9 = Res.irq(32 + 0x58, Io.Resource.Irq_type_level_high);
  end) -- dma@7e007000

  ethernet_7d580000 = Io.Hw.Device(function()
    compatible    = { "brcm,bcm2711-genet-v5", "brcm,genet-v5" };
    Property.flags = Io.Hw_device_DF_dma_supported;
    Resource.reg0 = Res.mmio(0xfd580000, 0xfd580000 + 0x10000 - 1);
    Resource.irq0 = Res.irq(32 + 0x9d, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32 + 0x9e, Io.Resource.Irq_type_level_high);
  end)

  pcie_7d500000 = Io.Hw.Device(function()
    compatible    = { "brcm,bcm2711-pcie" };
    --                      [0x7d500000, 0x00009310]
    Resource.reg0 = Res.mmio(0xfd500000, 0xfd500000 + 0x00009310 - 1);
    -- from ranges [ 0xf8000000 -> 0x600000000, 0x4000000 ]
    Resource.reg1 = Res.mmio(0x600000000, 0x600000000 + 0x4000000 - 1);
    Resource.irq0 = Res.irq(32 + 0x93, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32 + 0x94, Io.Resource.Irq_type_level_high);
  end)

end)
