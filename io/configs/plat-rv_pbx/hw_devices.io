-- vi:ft=lua

-- ARM Realview PBX Platform

local Res = Io.Res
local Hw = Io.Hw

Io.hw_add_devices(function()

  CTRL = Hw.Device(function()
    Property.hid = "System Control";
    compatible = {"arm,sysctl"};
    Resource.regs = Res.mmio(0x10000000, 0x10000fff);
  end);

  LCD = Hw.Device(function()
    Property.hid = "AMBA PL110";
    compatible = {"arm,pl111","arm,primecell"};
    Resource.regs = Res.mmio(0x10020000, 0x10020fff);
    Resource.irq = Res.irq(55);
  end);

  KBD = Hw.Device(function()
    Property.hid = "AMBA KMI Kbd";
    compatible = {"arm,pl050","arm,primecell"};
    Resource.irq = Res.irq(52);
    Resource.regs = Res.mmio(0x10006000, 0x10006fff);
  end);

  MOUSE = Hw.Device(function()
    Property.hid = "AMBA KMI mou";
    compatible = {"arm,pl050","arm,primecell"};
    Resource.regs = Res.mmio(0x10007000, 0x10007fff);
    Resource.irq = Res.irq(53);
  end);

  GPIO0 = Hw.Device(function()
    Property.hid = "AMBA PL061 dev0";
    compatible = {"arm,pl061", "arm,primecell"};
    Resource.regs = Res.mmio(0x10013000, 0x10013fff);
    Resource.irq = Res.irq(38);
  end);

  GPIO1 = Hw.Device(function()
    Property.hid = "AMBA PL061 dev1";
    compatible = {"arm,pl061", "arm,primecell"};
    Resource.regs = Res.mmio(0x10014000, 0x10014fff);
    Resource.irq = Res.irq(39);
  end);

  GPIO2 = Hw.Device(function()
    Property.hid = "AMBA PL061 dev2";
    compatible = {"arm,pl061", "arm,primecell"};
    Resource.regs = Res.mmio(0x10015000, 0x10015fff);
    Resource.irq = Res.irq(40);
  end);

  COMPACTFLASH = Hw.Device(function()
    Property.hid = "compactflash"; -- FIXME: should be something like "ARM RV FLASH"
    Resource.regs = Res.mmio(0x18000000, 0x1b000fff);
    Resource.irq = Res.irq(59);
  end);

  AACI = Hw.Device(function()
    Property.hid = "aaci";
    compatible = {"arm,pl041", "arm,primecell"};
    Resource.regs = Res.mmio(0x10004000, 0x10004fff);
    Resource.irq = Res.irq(51);
  end);

  NIC = Hw.Device(function()
    Property.hid = "smsc911x";
    Resource.regs = Res.mmio(0x4e000000, 0x4e000fff);
    Resource.irq = Res.irq(60);
  end);

  USB = Hw.Device(function()
    Property.hid = "usb";
    Resource.regs = Res.mmio(0x4f000000, 0x4fffffff);
    Resource.irq = Res.irq(61);
  end);

  RTC = Hw.Device(function()
    Property.hid = "rtc";
    compatible = {"arm,pl031"};
    Resource.regs = Res.mmio(0x10017000, 0x10017fff);
    Resource.irq = Res.irq(42);
  end);
end)
