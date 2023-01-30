#pragma once

#include "arm_dma_domain.h"
#include "hw_device.h"
#include "main.h"

namespace Hw {
  /**
   * Hw::Device with specific properties for ARM IOMMUs.
   *
   * This class offers the following new properties for devices:
   *   - iommu   ID of the IOMMU
   *   - sid     stream ID of the device
   *
   * FIXME: Using an extra class for this seems like an unsatisfactory solution.
   */
  class Arm_dma_device : public Device
  {
  public:
    Arm_dma_device(l4_umword_t uid, l4_uint32_t adr) : Device(uid, adr)
    { setup(); }

    Arm_dma_device(l4_uint32_t adr) : Device(adr)
    { setup(); }

    Arm_dma_device() : Device()
    { setup(); }

  private:
    void setup()
    {
      register_property("iommu", &_iommu);
      register_property("sid", &_sid);

      property("flags")->set(-1, DF_dma_supported);
    }

    Int_property _iommu;
    Int_property _sid;
  };
}
