-- vim:set ft=lua:
--
-- SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom
-- Copyright (C) 2021-2023 Stephan Gerhold <stephan@gerhold.net>
--
-- Qualcomm Snapdragon 800 (MSM8974)

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()

  TLMM = Hw.Gpio_qcom_chip(function()
    compatible = {"qcom,msm8974-pinctrl", "qcom,tlmm"};
    Resource.reg0 = Res.mmio(0xfd511000, 0xfd513fff);
    Resource.irq0 = Res.irq(32+208, Io.Resource.Irq_type_level_high);
    Property.ngpios = 146;
    Property.target_proc = 0x4;
    Property.reg_gpio_size = 0x10;
  end);

  MDSS = Hw.Device(function()
    compatible = {"qcom,msm8974-mdss", "qcom,mdss"};
    Resource.reg0 = Res.mmio(0xfd900000, 0xfd9fffff);
    Resource.irq0 = Res.irq(32+72, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
    Child.MDP5 = Hw.Device(function()
      compatible = {"qcom,msm8974-mdp5", "qcom,mdp5"};
      Resource.reg0 = Res.mmio(0xfd900100, 0xfd9220ff);
      Property.flags = Io.Hw_device_DF_dma_supported;
      Property.hid = "qcom,mdp5"; -- Used by lcd driver
    end);
  end);

end)
