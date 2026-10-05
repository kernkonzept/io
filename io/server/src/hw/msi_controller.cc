/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "msi_controller.h"

namespace
{
static Hw::Device_factory_t<Hw::Msi_controller> __hw_msi_controller_factory("Msi_controller");
}
