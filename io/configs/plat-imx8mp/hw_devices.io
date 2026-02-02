-- vi:ft=lua

local hw = Io.system_bus()

local type_map = {
   [0] = Io.Resource.Irq_type_none,
   [1] = Io.Resource.Irq_type_raising_edge,
   [2] = Io.Resource.Irq_type_falling_edge,
   [3] = Io.Resource.Irq_type_both_edges,
   [4] = Io.Resource.Irq_type_level_high,
   [8] = Io.Resource.Irq_type_level_low
}

function reg_irq(spi, irq, irq_type)
   if spi ~= 0 then
      error("Unable to handle non SPI irqs", 2);
   end
   if type_map[irq_type] == nil then
      error("Undefined interrupt type " .. irq_type, 2);
   end
   return Io.Res.irq(irq + 32, type_map[irq_type]);
end

function reg_mmio(base, size)
   return Io.Res.mmio(base, base + size - 1)
end

Io.hw_add_devices(function()
-- / (0) ignored
  -- memory@40000000 (1) ignored
      -- /reserved-memory/ocram@900000 (2)
      ocram_900000 = Io.Hw.Device(function()
         Resource.reg0 = reg_mmio(0x00900000, 0x00070000);
      end) -- ocram@900000
      -- /reserved-memory/gpu_reserved@100000000 (2)
      gpu_reserved_100000000 = Io.Hw.Device(function()
         Resource.reg0 = reg_mmio(0x100000000, 0x10000000);
      end) -- gpu_reserved@100000000
      -- /reserved-memory/dsp_reserved_heap@93400000 (2)
      dsp_reserved_heap_93400000 = Io.Hw.Device(function()
         Resource.reg0 = reg_mmio(0x93400000, 0x00ef0000);
      end) -- dsp_reserved_heap@93400000
      -- /reserved-memory/m7@77000000 (2)
      m7_77000000 = Io.Hw.Device(function()
         -- disabled
         Resource.reg0 = reg_mmio(0x77000000, 0x01000000);
      end) -- m7@77000000
      -- /reserved-memory/vdev0vring0@55000000 (2)
      vdev0vring0_55000000 = Io.Hw.Device(function()
         -- disabled
         Resource.reg0 = reg_mmio(0x55000000, 0x00008000);
      end) -- vdev0vring0@55000000
      -- /reserved-memory/vdev0vring1@55008000 (2)
      vdev0vring1_55008000 = Io.Hw.Device(function()
         -- disabled
         Resource.reg0 = reg_mmio(0x55008000, 0x00008000);
      end) -- vdev0vring1@55008000
      -- /reserved-memory/rsc-table@550ff000 (2)
      rsc_table_550ff000 = Io.Hw.Device(function()
         -- disabled
         Resource.reg0 = reg_mmio(0x550ff000, 0x00001000);
      end) -- rsc-table@550ff000
      -- /reserved-memory/vdevbuffer@55400000 (2)
      vdevbuffer_55400000 = Io.Hw.Device(function()
         -- disabled
         compatible    = { "shared-dma-pool" };
         Resource.reg0 = reg_mmio(0x55400000, 0x00100000);
      end) -- vdevbuffer@55400000
  -- /pmu (1)
--  pmu = Io.Hw.Device(function()
--      compatible    = { "arm,cortex-a53-pmu" };
--      Resource.irq0 = reg_irq(0x1, 0x7, 0xf04);
--  end) -- pmu
  -- timer (1) ignored
  -- /caam_secvio (1)
  caam_secvio = Io.Hw.Device(function()
      compatible    = { "fsl,imx6q-caam-secvio" };
      Resource.irq0 = reg_irq(0x0, 0x14, 0x4);
  end) -- caam_secvio
      -- /soc@0/caam-sm@100000 (2)
      caam_sm_100000 = Io.Hw.Device(function()
        compatible    = { "fsl,imx6q-caam-sm" };
        Resource.reg0 = reg_mmio(0x00100000, 0x00008000);
      end) -- caam-sm@100000
         -- /soc@0/bus@30000000/gpio@30200000 (3)
         gpio0 = Io.Hw.Gpio_imx_chip(function()
           compatible    = { "fsl,imx8mp-gpio", "fsl,imx35-gpio" };
           Resource.regs = reg_mmio(0x30200000, 0x00001000);
           Resource.irq0 = reg_irq(0x0, 0x40, 0x4);
           Resource.irq1 = reg_irq(0x0, 0x41, 0x4);
         end) -- gpio@30200000
         -- /soc@0/bus@30000000/gpio@30210000 (3)
         gpio1 = Io.Hw.Gpio_imx_chip(function()
            compatible    = { "fsl,imx8mp-gpio", "fsl,imx35-gpio" };
            Resource.regs = reg_mmio(0x30210000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x42, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x43, 0x4);
         end) -- gpio@30210000
         -- /soc@0/bus@30000000/gpio@30220000 (3)
         gpio2 = Io.Hw.Gpio_imx_chip(function()
            compatible    = { "fsl,imx8mp-gpio", "fsl,imx35-gpio" };
            Resource.regs = reg_mmio(0x30220000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x44, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x45, 0x4);
         end) -- gpio@30220000
         -- /soc@0/bus@30000000/gpio@30230000 (3)
         gpio3 = Io.Hw.Gpio_imx_chip(function()
            compatible    = { "fsl,imx8mp-gpio", "fsl,imx35-gpio" };
            Resource.regs = reg_mmio(0x30230000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x46, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x47, 0x4);
         end) -- gpio@30230000
         -- /soc@0/bus@30000000/gpio@30240000 (3)
         gpio4 = Io.Hw.Gpio_imx_chip(function()
            compatible    = { "fsl,imx8mp-gpio", "fsl,imx35-gpio" };
            Resource.regs = reg_mmio(0x30240000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x48, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x49, 0x4);
         end) -- gpio@30240000
         -- /soc@0/bus@30000000/tmu@30260000 (3)
         tmu_30260000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-tmu" };
            Resource.reg0 = reg_mmio(0x30260000, 0x00010000);
         end) -- tmu@30260000
         -- /soc@0/bus@30000000/watchdog@30280000 (3)
         watchdog_30280000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-wdt", "fsl,imx21-wdt" };
            Resource.reg0 = reg_mmio(0x30280000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x4e, 0x4);
         end) -- watchdog@30280000
         -- /soc@0/bus@30000000/watchdog@30290000 (3)
         watchdog_30290000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-wdt", "fsl,imx21-wdt" };
            Resource.reg0 = reg_mmio(0x30290000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x4f, 0x4);
         end) -- watchdog@30290000
         -- /soc@0/bus@30000000/watchdog@302a0000 (3)
         watchdog_302a0000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-wdt", "fsl,imx21-wdt" };
            Resource.reg0 = reg_mmio(0x302a0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0xa, 0x4);
         end) -- watchdog@302a0000
         -- /soc@0/bus@30000000/timer@302d0000 (3)
         timer_302d0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x302d0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x37, 0x4);
         end) -- timer@302d0000
         -- /soc@0/bus@30000000/timer@302e0000 (3)
         timer_302e0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x302e0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x36, 0x4);
         end) -- timer@302e0000
         -- /soc@0/bus@30000000/timer@302f0000 (3)
         timer_302f0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x302f0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x35, 0x4);
         end) -- timer@302f0000
         -- /soc@0/bus@30000000/pinctrl@30330000 (3)


         pinctrl_30330000 = Io.Hw.Iomuxc_imx8mp(function()
            compatible    = { "fsl,imx8mp-iomuxc" };
            Resource.reg0 = reg_mmio(0x30330000, 0x00010000);
--            Resource.reg1 = reg_mmio(0x30340000, 0x00010000); -- gpr
         end) -- pinctrl@30330000


         -- /soc@0/bus@30000000/syscon@30340000 (3)
         syscon_30340000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-iomuxc-gpr", "syscon" };
            Resource.reg0 = reg_mmio(0x30340000, 0x00010000);
         end) -- syscon@30340000
         -- /soc@0/bus@30000000/efuse@30350000 (3)
         efuse_30350000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-ocotp", "fsl,imx8mm-ocotp", "syscon", "simple-mfd" };
            Resource.reg0 = reg_mmio(0x30350000, 0x00010000);
         end) -- efuse@30350000
         -- /soc@0/bus@30000000/clock-controller@30360000 (3)
         clock_controller_30360000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-anatop", "fsl,imx8mm-anatop" };
            Resource.reg0 = reg_mmio(0x30360000, 0x00010000);
         end) -- clock-controller@30360000
         -- /soc@0/bus@30000000/caam-snvs@30370000 (3)
         caam_snvs_30370000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx6q-caam-snvs" };
            Resource.reg0 = reg_mmio(0x30370000, 0x00010000);
         end) -- caam-snvs@30370000
         -- /soc@0/bus@30000000/snvs@30370000 (3)
         snvs_30370000 = Io.Hw.Device(function()
            compatible    = { "fsl,sec-v4.0-mon", "syscon", "simple-mfd" };
            Resource.reg0 = reg_mmio(0x30370000, 0x00010000);
            -- /soc@0/bus@30000000/snvs@30370000/snvs-rtc-lp (4)
            snvs_rtc_lp = Io.Hw.Device(function()
              compatible    = { "fsl,sec-v4.0-mon-rtc-lp" };
              Resource.irq0 = reg_irq(0x0, 0x13, 0x4);
              Resource.irq1 = reg_irq(0x0, 0x14, 0x4);
            end) -- snvs-rtc-lp
            -- /soc@0/bus@30000000/snvs@30370000/snvs-powerkey (4)
            snvs_powerkey = Io.Hw.Device(function()
              compatible    = { "fsl,sec-v4.0-pwrkey" };
              Resource.irq0 = reg_irq(0x0, 0x4, 0x4);
            end) -- snvs-powerkey
         end) -- snvs@30370000
         -- /soc@0/bus@30000000/clock-controller@30380000 (3)
         clock_controller_30380000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-ccm" };
            Resource.reg0 = reg_mmio(0x30380000, 0x00010000);
         end) -- clock-controller@30380000
         -- /soc@0/bus@30000000/reset-controller@30390000 (3)
         reset_controller_30390000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-src", "syscon" };
            Resource.reg0 = reg_mmio(0x30390000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x59, 0x4);
         end) -- reset-controller@30390000
         -- /soc@0/bus@30000000/gpc@303a0000 (3)
         gpc_303a0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpc" };
            Resource.reg0 = reg_mmio(0x303a0000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x57, 0x4);
         end) -- gpc@303a0000
         -- /soc@0/bus@30400000/pwm@30660000 (3)
         pwm_30660000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-pwm", "fsl,imx27-pwm" };
            Resource.reg0 = reg_mmio(0x30660000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x51, 0x4);
         end) -- pwm@30660000
         -- /soc@0/bus@30400000/pwm@30670000 (3)
         pwm_30670000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-pwm", "fsl,imx27-pwm" };
            Resource.reg0 = reg_mmio(0x30670000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x52, 0x4);
         end) -- pwm@30670000
         -- /soc@0/bus@30400000/pwm@30680000 (3)
         pwm_30680000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-pwm", "fsl,imx27-pwm" };
            Resource.reg0 = reg_mmio(0x30680000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x53, 0x4);
         end) -- pwm@30680000
         -- /soc@0/bus@30400000/pwm@30690000 (3)
         pwm_30690000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-pwm", "fsl,imx27-pwm" };
            Resource.reg0 = reg_mmio(0x30690000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x54, 0x4);
         end) -- pwm@30690000
         -- /soc@0/bus@30400000/timer@306a0000 (3)
         timer_306a0000 = Io.Hw.Device(function()
            compatible    = { "nxp,sysctr-timer" };
            Resource.reg0 = reg_mmio(0x306a0000, 0x00020000);
            Resource.irq0 = reg_irq(0x0, 0x2f, 0x4);
         end) -- timer@306a0000
         -- /soc@0/bus@30400000/timer@306e0000 (3)
         timer_306e0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x306e0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x33, 0x4);
         end) -- timer@306e0000
         -- /soc@0/bus@30400000/timer@306f0000 (3)
         timer_306f0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x306f0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x33, 0x4);
         end) -- timer@306f0000
         -- /soc@0/bus@30400000/timer@30700000 (3)
         timer_30700000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-gpt", "fsl,imx6dl-gpt" };
            Resource.reg0 = reg_mmio(0x30700000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x34, 0x4);
         end) -- timer@30700000
            -- /soc@0/bus@30800000/spba-bus@30800000/spi@30820000 (4)
            spi_30820000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-ecspi", "fsl,imx6ul-ecspi" };
              Resource.reg0 = reg_mmio(0x30820000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x1f, 0x4);
            end) -- spi@30820000
            -- /soc@0/bus@30800000/spba-bus@30800000/spi@30830000 (4)
            spi_30830000 = Io.Hw.Spi_imx8mp_ctrl(function()
              compatible    = { "fsl,imx8mp-ecspi", "fsl,imx6ul-ecspi" };
              Resource.reg0 = reg_mmio(0x30830000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x20, 0x4);
              Resource.cspin = Io.Gpio_resource(gpio4, 0xd,  0xd);
              Property.iomuxc = pinctrl_30330000;
              -- The `pads` property takes six values per pad:
              --    three fields offsets of the IOMUXC MMIO registers and
              --    three values to write to these offsets.
              -- The order is:
              --    field offsets: Mux, Config, Input
              --    values to set: Mux, Input, Config
              -- The difference in value order is in sync with the linux DT.
              Property.pads = { 0x1f8, 0x458, 0x56c, 0x00, 0x01, 0x1c0,
                                0x1f4, 0x454, 0x570, 0x00, 0x01, 0x1c0,
                                0x1f0, 0x450, 0x568, 0x00, 0x01, 0x1c0,
                                0x1fc, 0x45c, 0x00,  0x05, 0x00, 0x1c0 };
              end) -- spi@30830000
            -- /soc@0/bus@30800000/spba-bus@30800000/spi@30840000 (4)
            spi_30840000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-ecspi", "fsl,imx6ul-ecspi" };
              Resource.reg0 = reg_mmio(0x30840000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x21, 0x4);
            end) -- spi@30840000
            -- /soc@0/bus@30800000/spba-bus@30800000/serial@30860000 (4)
            serial_30860000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-uart", "fsl,imx6q-uart" };
              Resource.reg0 = reg_mmio(0x30860000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x1a, 0x4);
            end) -- serial@30860000
            -- /soc@0/bus@30800000/spba-bus@30800000/serial@30880000 (4)
            serial_30880000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-uart", "fsl,imx6q-uart" };
              Resource.reg0 = reg_mmio(0x30880000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x1c, 0x4);
            end) -- serial@30880000
            -- /soc@0/bus@30800000/spba-bus@30800000/serial@30890000 (4)
            serial_30890000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-uart", "fsl,imx6q-uart" };
              Resource.reg0 = reg_mmio(0x30890000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x1b, 0x4);
            end) -- serial@30890000
            -- /soc@0/bus@30800000/spba-bus@30800000/can@308c0000 (4)
            can_308c0000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-flexcan" };
              Resource.reg0 = reg_mmio(0x308c0000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x8e, 0x4);
            end) -- can@308c0000
            -- /soc@0/bus@30800000/spba-bus@30800000/can@308d0000 (4)
            can_308d0000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-flexcan" };
              Resource.reg0 = reg_mmio(0x308d0000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x90, 0x4);
            end) -- can@308d0000
         -- /soc@0/bus@30800000/crypto@30900000 (3)
         crypto_30900000 = Io.Hw.Device(function()
            compatible    = { "fsl,sec-v4.0" };
            Resource.reg0 = reg_mmio(0x30900000, 0x00040000);
            Resource.irq0 = reg_irq(0x0, 0x5b, 0x4);
         end) -- crypto@30900000
            -- /soc@0/bus@30800000/crypto@30900000/jr@1000 (4)
            jr_1000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,sec-v4.0-job-ring" };
              --                      [0x00001000, 0x00001000]
              Resource.reg0 = reg_mmio(0x30901000, 0x00001000);
              Resource.irq0 = reg_irq(0x0, 0x69, 0x4);
            end) -- jr@1000
            -- /soc@0/bus@30800000/crypto@30900000/jr@2000 (4)
            jr_2000 = Io.Hw.Device(function()
              compatible    = { "fsl,sec-v4.0-job-ring" };
              --                      [0x00002000, 0x00001000]
              Resource.reg0 = reg_mmio(0x30902000, 0x00001000);
              Resource.irq0 = reg_irq(0x0, 0x6a, 0x4);
            end) -- jr@2000
            -- /soc@0/bus@30800000/crypto@30900000/jr@3000 (4)
            jr_3000 = Io.Hw.Device(function()
              compatible    = { "fsl,sec-v4.0-job-ring" };
              --                      [0x00003000, 0x00001000]
              Resource.reg0 = reg_mmio(0x30903000, 0x00001000);
              Resource.irq0 = reg_irq(0x0, 0x72, 0x4);
            end) -- jr@3000
         -- /soc@0/bus@30800000/i2c@30a20000 (3)
         i2c_30a20000 = Io.Hw.I2c_imx8mp_ctrl(function()
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30a20000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x23, 0x4);
            Property.iomuxc = pinctrl_30330000;
            -- The `pads` property takes six values per pad:
            --    three fields offsets of the IOMUXC MMIO registers and
            --    three values to write to these offsets.
            -- The order is:
            --    field offsets: Mux, Config, Input
            --    values to set: Mux, Input, Config
            -- The difference in value order is in sync with the linux DT.
            Property.pads = { 0x200, 0x460, 0x5a4, 0x10, 0x02, 0x000001e2,
                              0x204, 0x464, 0x5a8, 0x10, 0x02, 0x000001e2 };
            -- /soc@0/bus@30800000/i2c@30a20000/pmic@25 (4)
            pmic_25 = Io.Hw.Device(function()
              compatible    = { "nxp,pca9450c" };
              -- Resource.reg0 not translatable
               --       [ ( 0x00000025 ) ( ) ]
              -- ignore IRQs routed to interrupt-parent gpio@30200000
              -- Resource.irq0 = reg_irq(0x8, 0x8);
            end) -- pmic@25
            -- /soc@0/bus@30800000/i2c@30a20000/rtc@51 (4)
            rtc_51 = Io.Hw.Device(function()
              compatible    = { "nxp,pcf85063a" };
              -- Resource.reg0 not translatable
                --      [ ( 0x00000051 ) ( ) ]
              -- ignore IRQs routed to interrupt-parent gpio@30230000
              -- Resource.irq0 = reg_irq(0x1c, 0x2);
            end) -- rtc@51
         end) -- i2c@30a20000
         -- /soc@0/bus@30800000/i2c@30a30000 (3)
         i2c_30a30000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30a30000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x24, 0x4);
         end) -- i2c@30a30000
         -- /soc@0/bus@30800000/i2c@30a40000 (3)
         i2c_30a40000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30a40000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x25, 0x4);
         end) -- i2c@30a40000
         -- /soc@0/bus@30800000/i2c@30a50000 (3)
         i2c_30a50000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30a50000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x26, 0x4);
         end) -- i2c@30a50000
         -- /soc@0/bus@30800000/serial@30a60000 (3)
         serial_30a60000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-uart", "fsl,imx6q-uart" };
            Resource.reg0 = reg_mmio(0x30a60000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x1d, 0x4);
         end) -- serial@30a60000
         -- /soc@0/bus@30800000/mailbox@30aa0000 (3)
         mailbox_30aa0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-mu", "fsl,imx6sx-mu" };
            Resource.reg0 = reg_mmio(0x30aa0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x58, 0x4);
         end) -- mailbox@30aa0000
         -- /soc@0/bus@30800000/mailbox@30e60000 (3)
         mailbox_30e60000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8-mu-dsp", "fsl,imx6sx-mu" };
            Resource.reg0 = reg_mmio(0x30e60000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x88, 0x4);
         end) -- mailbox@30e60000
         -- /soc@0/bus@30800000/i2c@30ad0000 (3)
         i2c_30ad0000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30ad0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x4c, 0x4);
         end) -- i2c@30ad0000
         -- /soc@0/bus@30800000/i2c@30ae0000 (3)
         i2c_30ae0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-i2c", "fsl,imx21-i2c" };
            Resource.reg0 = reg_mmio(0x30ae0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x4d, 0x4);
         end) -- i2c@30ae0000
         -- /soc@0/bus@30800000/mmc@30b40000 (3)
         mmc_30b40000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-usdhc", "fsl,imx8mm-usdhc", "fsl,imx7d-usdhc" };
            Resource.reg0 = reg_mmio(0x30b40000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x16, 0x4);
         end) -- mmc@30b40000
         -- /soc@0/bus@30800000/mmc@30b50000 (3)
         mmc_30b50000 = Io.Hw.Arm_dma_device(function()
            compatible    = { "fsl,imx8mp-usdhc", "fsl,imx8mm-usdhc", "fsl,imx7d-usdhc" };
            Resource.reg0 = reg_mmio(0x30b50000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x17, 0x4);
         end) -- mmc@30b50000
         -- /soc@0/bus@30800000/mmc@30b60000 (3)
         mmc_30b60000 = Io.Hw.Arm_dma_device(function()
            compatible    = { "fsl,imx8mp-usdhc", "fsl,imx8mm-usdhc", "fsl,imx7d-usdhc" };
            Resource.reg0 = reg_mmio(0x30b60000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x18, 0x4);
         end) -- mmc@30b60000
         -- /soc@0/bus@30800000/spi@30bb0000 (3)
         spi_30bb0000 = Io.Hw.Device(function()
            compatible    = { "nxp,imx8mp-fspi" };
            Resource.reg0 = reg_mmio(0x30bb0000, 0x00010000);
            Resource.reg1 = reg_mmio(0x08000000, 0x10000000);
            Resource.irq0 = reg_irq(0x0, 0x6b, 0x4);
         end) -- spi@30bb0000
         -- /soc@0/bus@30800000/dma-controller@30bd0000 (3)
         dma_controller_30bd0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mq-sdma" };
            Resource.reg0 = reg_mmio(0x30bd0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x2, 0x4);
         end) -- dma-controller@30bd0000
         -- /soc@0/bus@30800000/ethernet@30be0000 (3)
         ethernet_30be0000 = Io.Hw.Arm_dma_device(function()
            compatible    = { "fsl,imx8mp-fec", "fsl,imx8mq-fec", "fsl,imx6sx-fec" };
            Resource.reg0 = reg_mmio(0x30be0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x76, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x77, 0x4);
            Resource.irq2 = reg_irq(0x0, 0x78, 0x4);
            Resource.irq3 = reg_irq(0x0, 0x79, 0x4);
              -- /soc@0/bus@30800000/ethernet@30be0000/mdio/ethernet-phy@0 (5)
              ethernet_phy_0 = Io.Hw.Device(function()
              compatible    = { "ethernet-phy-ieee802.3-c22" };
              -- Resource.reg0 not translatable
               --       [ ( ) ( ) ]
              -- ignore IRQs routed to interrupt-parent gpio@30230000
              -- Resource.irq0 = reg_irq(0x1, 0x2);
              end) -- ethernet-phy@0
         end) -- ethernet@30be0000
         -- /soc@0/bus@30800000/ethernet@30bf0000 (3)
         ethernet_30bf0000 = Io.Hw.Arm_dma_device(function()
            compatible    = { "nxp,imx8mp-dwmac-eqos", "snps,dwmac-5.10a" };
            Resource.reg0 = reg_mmio(0x30bf0000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x87, 0x4);
            Resource.irq1 = reg_irq(0x0, 0x86, 0x4);
              -- /soc@0/bus@30800000/ethernet@30bf0000/mdio/ethernet-phy@3 (5)
              ethernet_phy_3 = Io.Hw.Device(function()
              compatible    = { "ethernet-phy-ieee802.3-c22" };
              -- Resource.reg0 not translatable
               --       [ ( 0x00000003 ) ( ) ]
              -- ignore IRQs routed to interrupt-parent gpio@30230000
              -- Resource.irq0 = reg_irq(0x3, 0x2);
              end) -- ethernet-phy@3
         end) -- ethernet@30bf0000
      -- /soc@0/interconnect@32700000 (2)
      interconnect_32700000 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-noc", "fsl,imx8m-noc", "syscon" };
         Resource.reg0 = reg_mmio(0x32700000, 0x00100000);
      end) -- interconnect@32700000
         -- /soc@0/bus@32c00000/mipi_dsi@32e60000 (3)
         mipi_dsi_32e60000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-mipi-dsim" };
            Resource.reg0 = reg_mmio(0x32e60000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x12, 0x4);
         end) -- mipi_dsi@32e60000
         -- /soc@0/bus@32c00000/lcd-controller@32e80000 (3)
         lcd_controller_32e80000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-lcdif1" };
            Resource.reg0 = reg_mmio(0x32e80000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x5, 0x4);
         end) -- lcd-controller@32e80000
         -- /soc@0/bus@32c00000/lcd-controller@32e90000 (3)
         lcd_controller_32e90000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-lcdif2" };
            Resource.reg0 = reg_mmio(0x32e90000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x6, 0x4);
         end) -- lcd-controller@32e90000
         -- /soc@0/bus@32c00000/blk-ctrl@32ec0000 (3)
         blk_ctrl_32ec0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-media-blk-ctrl", "syscon" };
            Resource.reg0 = reg_mmio(0x32ec0000, 0x00010000);
         end) -- blk-ctrl@32ec0000
         -- /soc@0/bus@32c00000/pcie-phy@32f00000 (3)
         pcie_phy_32f00000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-pcie-phy" };
            Resource.reg0 = reg_mmio(0x32f00000, 0x00010000);
         end) -- pcie-phy@32f00000
         -- /soc@0/bus@32c00000/blk-ctrl@32f10000 (3)
         blk_ctrl_32f10000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-hsio-blk-ctrl", "syscon" };
            Resource.reg0 = reg_mmio(0x32f10000, 0x00000024);
         end) -- blk-ctrl@32f10000
         -- /soc@0/bus@32c00000/blk-ctrl@32fc0000 (3)
         blk_ctrl_32fc0000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-hdmi-blk-ctrl", "syscon" };
            Resource.reg0 = reg_mmio(0x32fc0000, 0x0000023c);
         end) -- blk-ctrl@32fc0000
         -- /soc@0/bus@32c00000/media_gpr@32ec0008 (3)
         media_gpr_32ec0008 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-iomuxc-gpr", "syscon" };
            Resource.reg0 = reg_mmio(0x32ec0008, 0x00000004);
         end) -- media_gpr@32ec0008
         -- /soc@0/bus@32c00000/gasket@32ec0060 (3)
         gasket_32ec0060 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-iomuxc-gpr", "syscon" };
            Resource.reg0 = reg_mmio(0x32ec0060, 0x00000028);
         end) -- gasket@32ec0060
         -- /soc@0/bus@32c00000/gasket@32ec0090 (3)
         gasket_32ec0090 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-iomuxc-gpr", "syscon" };
            Resource.reg0 = reg_mmio(0x32ec0090, 0x00000028);
         end) -- gasket@32ec0090
         -- /soc@0/bus@32c00000/isi_chain@32e02000 (3)
         isi_chain_32e02000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-iomuxc-gpr", "syscon" };
            Resource.reg0 = reg_mmio(0x32e02000, 0x00000004);
         end) -- isi_chain@32e02000
            -- /soc@0/bus@32c00000/camera/isi@32e00000 (4)
            isi_32e00000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-isi", "fsl,imx8mn-isi" };
              Resource.reg0 = reg_mmio(0x32e00000, 0x00002000);
              Resource.irq0 = reg_irq(0x0, 0x10, 0x4);
            end) -- isi@32e00000
            -- /soc@0/bus@32c00000/camera/isi@32e02000 (4)
            isi_32e02000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-isi", "fsl,imx8mn-isi" };
              Resource.reg0 = reg_mmio(0x32e02000, 0x00002000);
              Resource.irq0 = reg_irq(0x0, 0x2a, 0x4);
            end) -- isi@32e02000
            -- /soc@0/bus@32c00000/camera/dwe@32e30000 (4)
            dwe_32e30000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-dwe" };
              Resource.reg0 = reg_mmio(0x32e30000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x64, 0x4);
            end) -- dwe@32e30000
            -- /soc@0/bus@32c00000/camera/isp@32e10000 (4)
            isp_32e10000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-isp" };
              Resource.reg0 = reg_mmio(0x32e10000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x4a, 0x4);
            end) -- isp@32e10000
            -- /soc@0/bus@32c00000/camera/isp@32e20000 (4)
            isp_32e20000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-isp" };
              Resource.reg0 = reg_mmio(0x32e20000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x4b, 0x4);
            end) -- isp@32e20000
            -- /soc@0/bus@32c00000/camera/csi@32e40000 (4)
            csi_32e40000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-mipi-csi", "fsl,imx8mn-mipi-csi" };
              Resource.reg0 = reg_mmio(0x32e40000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x11, 0x4);
            end) -- csi@32e40000
            -- /soc@0/bus@32c00000/camera/csi@32e50000 (4)
            csi_32e50000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-mipi-csi", "fsl,imx8mn-mipi-csi" };
              Resource.reg0 = reg_mmio(0x32e50000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x50, 0x4);
            end) -- csi@32e50000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c10000 (4)
            sai_30c10000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c10000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x5f, 0x4);
            end) -- sai@30c10000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c20000 (4)
            sai_30c20000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c20000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x60, 0x4);
            end) -- sai@30c20000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c30000 (4)
            sai_30c30000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c30000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x32, 0x4);
            end) -- sai@30c30000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c50000 (4)
            sai_30c50000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c50000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x5a, 0x4);
            end) -- sai@30c50000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c60000 (4)
            sai_30c60000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c60000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x5a, 0x4);
            end) -- sai@30c60000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/sai@30c80000 (4)
            sai_30c80000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-sai", "fsl,imx8mm-sai" };
              Resource.reg0 = reg_mmio(0x30c80000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x6f, 0x4);
            end) -- sai@30c80000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/easrc@30c90000 (4)
            easrc_30c90000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mn-easrc" };
              Resource.reg0 = reg_mmio(0x30c90000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x7a, 0x4);
            end) -- easrc@30c90000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/micfil@30ca0000 (4)
            micfil_30ca0000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-micfil" };
              Resource.reg0 = reg_mmio(0x30ca0000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x6d, 0x4);
              Resource.irq1 = reg_irq(0x0, 0x6e, 0x4);
              Resource.irq2 = reg_irq(0x0, 0x2c, 0x4);
              Resource.irq3 = reg_irq(0x0, 0x2d, 0x4);
            end) -- micfil@30ca0000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/aud2htx@30cb0000 (4)
            aud2htx_30cb0000 = Io.Hw.Device(function()
              compatible    = { "fsl,imx8mp-aud2htx" };
              Resource.reg0 = reg_mmio(0x30cb0000, 0x00010000);
              Resource.irq0 = reg_irq(0x0, 0x82, 0x4);
            end) -- aud2htx@30cb0000
            -- /soc@0/bus@30c00000/spba-bus@30c00000/xcvr@30cc0000 (4)
            xcvr_30cc0000 = Io.Hw.Device(function()
              -- disabled
              compatible    = { "fsl,imx8mp-xcvr" };
              Resource.reg0 = reg_mmio(0x30cc0000, 0x00000800);
              Resource.reg1 = reg_mmio(0x30cc0800, 0x00000400);
              Resource.reg2 = reg_mmio(0x30cc0c00, 0x00000080);
              Resource.reg3 = reg_mmio(0x30cc0e00, 0x00000080);
              Resource.irq0 = reg_irq(0x0, 0x80, 0x4);
              Resource.irq1 = reg_irq(0x0, 0x81, 0x4);
              Resource.irq2 = reg_irq(0x0, 0x92, 0x4);
            end) -- xcvr@30cc0000
         -- /soc@0/bus@30c00000/dma-controller@30e00000 (3)
         dma_controller_30e00000 = Io.Hw.Device(function()
            -- disabled
            compatible    = { "fsl,imx8mp-sdma", "fsl,imx7d-sdma" };
            Resource.reg0 = reg_mmio(0x30e00000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x22, 0x4);
         end) -- dma-controller@30e00000
         -- /soc@0/bus@30c00000/dma-controller@30e10000 (3)
         dma_controller_30e10000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-sdma", "fsl,imx7d-sdma" };
            Resource.reg0 = reg_mmio(0x30e10000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x67, 0x4);
         end) -- dma-controller@30e10000
         -- /soc@0/bus@30c00000/audio-blk-ctrl@30e20000 (3)
         audio_blk_ctrl_30e20000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-audio-blk-ctrl", "syscon" };
            Resource.reg0 = reg_mmio(0x30e20000, 0x0000050c);
         end) -- audio-blk-ctrl@30e20000
         -- /soc@0/bus@30c00000/irqsteer@32fc2000 (3)
         irqsteer_32fc2000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx-irqsteer" };
            Resource.reg0 = reg_mmio(0x32fc2000, 0x00001000);
            Resource.irq0 = reg_irq(0x0, 0x2b, 0x4);
         end) -- irqsteer@32fc2000
         -- /soc@0/bus@30c00000/hdmi-pai-pvi@32fc4000 (3)
         hdmi_pai_pvi_32fc4000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-hdmi-pavi" };
            Resource.reg0 = reg_mmio(0x32fc4000, 0x00001000);
         end) -- hdmi-pai-pvi@32fc4000
         -- /soc@0/bus@30c00000/lcd-controller@32fc6000 (3)
         lcd_controller_32fc6000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-lcdif1" };
            Resource.reg0 = reg_mmio(0x32fc6000, 0x00010000);
            -- ignore IRQs routed to interrupt-parent irqsteer@32fc2000
            -- Resource.irq0 = reg_irq(0x8);
            -- Resource.irq1 = reg_irq(0x4);
         end) -- lcd-controller@32fc6000
         -- /soc@0/bus@30c00000/hdmi@32fd8000 (3)
         hdmi_32fd8000 = Io.Hw.Device(function()
            compatible    = { "fsl,imx8mp-hdmi" };
            Resource.reg0 = reg_mmio(0x32fd8000, 0x00007eff);
            -- ignore IRQs routed to interrupt-parent irqsteer@32fc2000
            -- Resource.irq0 = reg_irq(0x0);
            -- Resource.irq1 = reg_irq(0x4);
         end) -- hdmi@32fd8000
         -- /soc@0/bus@30c00000/hdmiphy@32fdff00 (3)
         hdmiphy_32fdff00 = Io.Hw.Device(function()
            compatible    = { "fsl,samsung-hdmi-phy" };
            Resource.reg0 = reg_mmio(0x32fdff00, 0x00000100);
         end) -- hdmiphy@32fdff00
      -- /soc@0/dma-apbh@33000000 (2)
      dma_apbh_33000000 = Io.Hw.Device(function()
         compatible    = { "fsl,imx7d-dma-apbh", "fsl,imx28-dma-apbh" };
         Resource.reg0 = reg_mmio(0x33000000, 0x00002000);
         Resource.irq0 = reg_irq(0x0, 0xc, 0x4);
      end) -- dma-apbh@33000000
      -- /soc@0/gpmi-nand@33002000 (2)
      gpmi_nand_33002000 = Io.Hw.Device(function()
         -- disabled
         compatible    = { "fsl,imx7d-gpmi-nand" };
         Resource.reg0 = reg_mmio(0x33002000, 0x00002000);
         Resource.reg1 = reg_mmio(0x33004000, 0x00004000);
         Resource.irq0 = reg_irq(0x0, 0xe, 0x4);
      end) -- gpmi-nand@33002000
      -- /soc@0/pcie@33800000 (2)
      pcie_33800000 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-pcie" };
         Resource.reg0 = reg_mmio(0x33800000, 0x00400000);
         Resource.reg1 = reg_mmio(0x1ff00000, 0x00080000);
         Resource.irq0 = reg_irq(0x0, 0x8c, 0x4);
      end) -- pcie@33800000
      -- /soc@0/pcie-ep@33800000 (2)
      pcie_ep_33800000 = Io.Hw.Device(function()
         -- disabled
         compatible    = { "fsl,imx8mp-pcie-ep" };
         Resource.reg0 = reg_mmio(0x33800000, 0x00100000);
         Resource.reg1 = reg_mmio(0x33900000, 0x00100000);
         Resource.reg2 = reg_mmio(0x33b00000, 0x00100000);
         Resource.reg3 = reg_mmio(0x18000000, 0x08000000);
         Resource.irq0 = reg_irq(0x0, 0x7f, 0x4);
      end) -- pcie-ep@33800000
      -- interrupt-controller@38800000 (2) ignored
      -- /soc@0/blk-ctl@38330000 (2)
      blk_ctl_38330000 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-vpu-blk-ctrl", "syscon" };
         Resource.reg0 = reg_mmio(0x38330000, 0x00000100);
      end) -- blk-ctl@38330000
      -- /soc@0/memory-controller@3d400000 (2)
      memory_controller_3d400000 = Io.Hw.Device(function()
         compatible    = { "snps,ddrc-3.80a" };
         Resource.reg0 = reg_mmio(0x3d400000, 0x00400000);
         Resource.irq0 = reg_irq(0x0, 0x93, 0x4);
      end) -- memory-controller@3d400000
      -- /soc@0/ddr-pmu@3d800000 (2)
      ddr_pmu_3d800000 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-ddr-pmu", "fsl,imx8m-ddr-pmu" };
         Resource.reg0 = reg_mmio(0x3d800000, 0x00400000);
         Resource.irq0 = reg_irq(0x0, 0x62, 0x4);
      end) -- ddr-pmu@3d800000
      -- /soc@0/usb-phy@381f0040 (2)
      usb_phy_381f0040 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-usb-phy" };
         Resource.reg0 = reg_mmio(0x381f0040, 0x00000040);
      end) -- usb-phy@381f0040
      -- /soc@0/usb@32f10100 (2)
      usb_32f10100 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-dwc3" };
         Resource.reg0 = reg_mmio(0x32f10100, 0x00000008);
         Resource.reg1 = reg_mmio(0x381f0000, 0x00000020);
         Resource.irq0 = reg_irq(0x0, 0x94, 0x4);
      end) -- usb@32f10100
         -- /soc@0/usb@32f10100/usb@38100000 (3)
         usb_38100000 = Io.Hw.Device(function()
            compatible    = { "snps,dwc3" };
            Resource.reg0 = reg_mmio(0x38100000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x28, 0x4);
         end) -- usb@38100000
      -- /soc@0/usb-phy@382f0040 (2)
      usb_phy_382f0040 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-usb-phy" };
         Resource.reg0 = reg_mmio(0x382f0040, 0x00000040);
      end) -- usb-phy@382f0040
      -- /soc@0/usb@32f10108 (2)
      usb_32f10108 = Io.Hw.Device(function()
         compatible    = { "fsl,imx8mp-dwc3" };
         Resource.reg0 = reg_mmio(0x32f10108, 0x00000008);
         Resource.reg1 = reg_mmio(0x382f0000, 0x00000020);
         Resource.irq0 = reg_irq(0x0, 0x95, 0x4);
      end) -- usb@32f10108
         -- /soc@0/usb@32f10108/usb@38200000 (3)
         usb_38200000 = Io.Hw.Device(function()
            compatible    = { "snps,dwc3" };
            Resource.reg0 = reg_mmio(0x38200000, 0x00010000);
            Resource.irq0 = reg_irq(0x0, 0x29, 0x4);
         end) -- usb@38200000
  -- /vpu_g1@38300000 (1)
  vpu_g1_38300000 = Io.Hw.Device(function()
      compatible    = { "nxp,imx8mm-hantro", "nxp,imx8mp-hantro" };
      Resource.reg0 = reg_mmio(0x38300000, 0x00100000);
      Resource.irq0 = reg_irq(0x0, 0x7, 0x4);
  end) -- vpu_g1@38300000
  -- /vpu_g2@38310000 (1)
  vpu_g2_38310000 = Io.Hw.Device(function()
      compatible    = { "nxp,imx8mm-hantro", "nxp,imx8mp-hantro" };
      Resource.reg0 = reg_mmio(0x38310000, 0x00100000);
      Resource.irq0 = reg_irq(0x0, 0x8, 0x4);
  end) -- vpu_g2@38310000
  -- /vpu_vc8000e@38320000 (1)
  vpu_vc8000e_38320000 = Io.Hw.Device(function()
      compatible    = { "nxp,imx8mp-hantro-vc8000e" };
      Resource.reg0 = reg_mmio(0x38320000, 0x00010000);
      Resource.irq0 = reg_irq(0x0, 0x1e, 0x4);
  end) -- vpu_vc8000e@38320000
  -- /gpu3d@38000000 (1)
  gpu3d_38000000 = Io.Hw.Arm_dma_device(function()
      compatible    = { "fsl,imx8-gpu" };
      Resource.reg0 = reg_mmio(0x38000000, 0x00008000);
      Resource.irq0 = reg_irq(0x0, 0x3, 0x4);
  end) -- gpu3d@38000000
  -- /gpu2d@38008000 (1)
  gpu2d_38008000 = Io.Hw.Device(function()
      compatible    = { "fsl,imx8-gpu" };
      Resource.reg0 = reg_mmio(0x38008000, 0x00008000);
      Resource.irq0 = reg_irq(0x0, 0x19, 0x4);
  end) -- gpu2d@38008000
  -- /vipsi@38500000 (1)
  vipsi_38500000 = Io.Hw.Device(function()
      compatible    = { "fsl,imx8-gpu", "fsl,imx8-vipsi" };
      Resource.reg0 = reg_mmio(0x38500000, 0x00020000);
      Resource.irq0 = reg_irq(0x0, 0xd, 0x4);
  end) -- vipsi@38500000
  -- /mix_gpu_ml@40000000 (1)
  mix_gpu_ml_40000000 = Io.Hw.Device(function()
      compatible    = { "fsl,imx8mp-gpu", "fsl,imx8-gpu-ss" };
      Resource.reg0 = reg_mmio(0x40000000, 0xc0000000);
      Resource.reg1 = reg_mmio(0x00000000, 0x10000000);
  end) -- mix_gpu_ml@40000000
  -- /dsp@3b6e8000 (1)
  dsp_3b6e8000 = Io.Hw.Device(function()
      -- disabled
      compatible    = { "fsl,imx8mp-hifi4" };
      Resource.reg0 = reg_mmio(0x3b6e8000, 0x00088000);
  end) -- dsp@3b6e8000
  -- /pwm-fan (1)
  pwm_fan = Io.Hw.Device(function()
      -- disabled
      compatible    = { "pwm-fan" };
      -- ignore IRQs routed to interrupt-parent gpio@30240000
      -- Resource.irq0 = reg_irq(0x12, 0x2);
  end) -- pwm-fan
end)
