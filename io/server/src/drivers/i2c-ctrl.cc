/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "debug.h"
#include "hw_device.h"
#include "irqs.h"

#include "iomuxc.h"


namespace {

class Hw_i2c_ctrl : public Hw::Device
{
public:
  void init() override;
};

void
Hw_i2c_ctrl::init()
{
  set_name_if_empty("Hw_i2c_ctrl");
}

class I2c_imx8mp_ctrl : public Hw::Iomux_device<Hw_i2c_ctrl>
{
};

static Hw::Device_factory_t<I2c_imx8mp_ctrl> __hw_pf_factory("I2c_imx8mp_ctrl");

}
