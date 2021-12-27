-- vim:set ft=lua:
--
-- SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom
-- Copyright (C) 2021-2023 Stephan Gerhold <stephan@gerhold.net>
--
-- Qualcomm Snapdragon 410 (MSM8916)

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()

  TLMM = Hw.Gpio_qcom_chip(function()
    compatible = {"qcom,msm8916-pinctrl", "qcom,tlmm"};
    Resource.reg0 = Res.mmio(0x01000000, 0x010fffff);
    Resource.irq0 = Res.irq(32+208, Io.Resource.Irq_type_level_high);
    Property.ngpios = 122;
    Property.target_proc = 0x4;
    Property.reg_gpio_size = 0x1000;
  end);

  GCC = Hw.Device(function()
    compatible = {"qcom,gcc-msm8916"};
    Resource.reg0 = Res.mmio(0x01800000, 0x0187ffff);

    -- Linux expects the clock controller to be represented as one large device,
    -- covering all clocks of the SoC. With minor driver changes it can be also
    -- partitioned into smaller parts that can be assigned separately to
    -- different virtual machines.
    Child.USB = Hw.Device(function()
      compatible = {"qcom,gcc-msm8916-usb"};
      Resource.reg0 = Res.mmio(0x01841000, 0x01841fff);
    end);
    Child.SDCC1 = Hw.Device(function()
      compatible = {"qcom,gcc-msm8916-sdcc1"};
      Resource.reg0 = Res.mmio(0x01842000, 0x01842fff);
    end);
    Child.SDCC2 = Hw.Device(function()
      compatible = {"qcom,gcc-msm8916-sdcc2"};
      Resource.reg0 = Res.mmio(0x01843000, 0x01843fff);
    end);
  end);

  MDSS = Hw.Arm_dma_device(function()
    compatible = {"qcom,msm8916-mdss", "qcom,mdss"};
    Resource.reg0 = Res.mmio(0x01a00000, 0x01afffff);
    Resource.irq0 = Res.irq(32+72, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
    Property.iommu = 0;
    Property.sid = 0xc00;
    Child.MDP5 = Hw.Arm_dma_device(function()
      compatible = {"qcom,msm8916-mdp5", "qcom,mdp5"};
      Resource.reg0 = Res.mmio(0x01a01000, 0x01a8ffff);
      Property.flags = Io.Hw_device_DF_dma_supported;
      Property.iommu = 0;
      Property.sid = 0xc00;
      Property.hid = "qcom,mdp5"; -- Used by lcd driver
    end);
  end);

  SDHC1 = Hw.Arm_dma_device(function()
    compatible = {"qcom,msm8916-sdhci", "qcom,sdhci-msm-v4"};
    Resource.reg0 = Res.mmio(0x07824000, 0x07824aff);
    Resource.irq0 = Res.irq(32+123, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32+138, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
    Property.iommu = 0;
    Property.sid = 0x100;
  end)
  SDHC2 = Hw.Arm_dma_device(function()
    compatible = {"qcom,msm8916-sdhci", "qcom,sdhci-msm-v4"};
    Resource.reg0 = Res.mmio(0x07864000, 0x07864aff);
    Resource.irq0 = Res.irq(32+125, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32+221, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
    Property.iommu = 0;
    Property.sid = 0x140;
  end)

  I2C1 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078b5000, 0x078b54ff);
    Resource.irq0 = Res.irq(32+95, Io.Resource.Irq_type_level_high);
  end);
  I2C2 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078b6000, 0x078b64ff);
    Resource.irq0 = Res.irq(32+96, Io.Resource.Irq_type_level_high);
  end);
  I2C3 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078b7000, 0x078b74ff);
    Resource.irq0 = Res.irq(32+97, Io.Resource.Irq_type_level_high);
  end);
  I2C4 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078b8000, 0x078b84ff);
    Resource.irq0 = Res.irq(32+98, Io.Resource.Irq_type_level_high);
  end);
  I2C5 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078b9000, 0x078b94ff);
    Resource.irq0 = Res.irq(32+99, Io.Resource.Irq_type_level_high);
  end);
  I2C6 = Hw.Device(function()
    compatible = {"qcom,i2c-qup-v2.2.1", "qcom,i2c-qup"};
    Resource.reg0 = Res.mmio(0x078ba000, 0x078ba4ff);
    Resource.irq0 = Res.irq(32+100, Io.Resource.Irq_type_level_high);
  end);

  USB = Hw.Arm_dma_device(function()
    compatible = {"qcom,ci-hdrc"};
    Resource.reg0 = Res.mmio(0x078d9000, 0x078d9fff);
    Resource.irq0 = Res.irq(32+134, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(32+140, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
    Property.iommu = 0;
    Property.sid = 0x2c0;
  end)

end)
