/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "debug.h"
#include "hw/hw_device.h"
#include "irqs.h"

#include "iomuxc.h"


namespace {

class Generic_iomux_dev : public Hw::Device
{
public:
  void init() override;
};

void
Generic_iomux_dev::init()
{
  set_name_if_empty("Hw_iomux_dev");
}

class Iomux_dev : public Hw::Iomux_device<Generic_iomux_dev>
{
};

static Hw::Device_factory_t<Iomux_dev> __hw_pf_factory("Iomux_dev");

}
