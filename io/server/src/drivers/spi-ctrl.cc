/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "debug.h"
#include "hw_device.h"
#include "irqs.h"

#include "iomuxc.h"


namespace {

class Hw_spi_ctrl : public Hw::Device
{
public:
  void init() override;
};

void
Hw_spi_ctrl::init()
{
  set_name_if_empty("Hw_spi_ctrl");
}

class Spi_imx8mp_ctrl : public Hw::Iomux_device<Hw_spi_ctrl>
{
};

static Hw::Device_factory_t<Spi_imx8mp_ctrl> __hw_pf_factory("Spi_imx8mp_ctrl");

}
