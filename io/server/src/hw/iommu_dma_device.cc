/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "iommu_dma_device.h"

namespace
{
static Hw::Device_factory_t<Hw::Iommu_dma_device> __hw_iommu_dma_device_factory("Iommu_dma_device");
}
