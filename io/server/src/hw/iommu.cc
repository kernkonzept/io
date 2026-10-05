/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "iommu.h"

namespace
{
static Hw::Device_factory_t<Hw::Iommu> __hw_iommu_factory("Iommu");
}
