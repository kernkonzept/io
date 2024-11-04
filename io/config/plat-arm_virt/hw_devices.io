-- vi:ft=lua

local Res = Io.Res
local Hw = Io.Hw

if string.packsize("T") * 8 == 64 then
  -- QEMU: VIRT_HIGH_PCIE_ECAM
  cfg_base     = 0x4010000000
  cfg_size     = 0x0010000000 -- 256 buses x 256 devs x 4KB
  -- QEMU: VIRT_HIGH_PCIE_MMIO
  mmio_base_64 = 0x8000000000
  mmio_size_64 = 0x8000000000 -- 512GB (for memory I/O access)
else
  -- see QEMU: VIRT_PCIE_ECAM
  cfg_base     = 0x3f000000
  cfg_size     = 0x01000000   -- 16 buses x 256 devs x 4KB
end

Io.hw_add_devices(function()

  pciec0 = Io.Hw.Ecam_pcie_bridge(function()
    -- QEMU: VIRT_PCIE_MMIO / VIRT_HIGH_PCIE_MMIO
    Property.regs_base    = 0x10000000
    Property.regs_size    = 0x2eff0000
    -- QEMU: VIRT_PCIE_ECAM / VIRT_HIGH_PCIE_ECAM
    Property.cfg_base     = cfg_base
    Property.cfg_size     = cfg_size
    -- QEMU: VIRT_PCIE_MMIO
    Property.mmio_base    = 0x10000000 -- 32-bit MMIO resources
    Property.mmio_size    = 0x2eff0000
    -- QEMU: VIRT_HIGH_PCIE_MMIO
    Property.mmio_base_64 = mmio_base_64
    Property.mmio_size_64 = mmio_size_64

    Property.int_a        = 32 + 3
    Property.int_b        = 32 + 4
    Property.int_c        = 32 + 5
    Property.int_d        = 32 + 6
  end);

  rtc = Hw.Device(function()
    compatible = {"arm,pl031"};
    Resource.req0 = Res.mmio(0x9010000, 0x9010fff);
    Resource.irq0 = Res.irq(32 + 2, Io.Resource.Irq_type_level_high);
  end);

end)
