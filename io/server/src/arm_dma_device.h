#pragma once

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
   * Only a single stream ID is supported.
   */
  class Arm_dma_device : public Device, public Dma_src_feature
  {
  public:
    Arm_dma_device(l4_umword_t uid, l4_uint32_t adr) : Device(uid, adr)
    { setup_device(); }

    Arm_dma_device(l4_uint32_t adr) : Device(adr)
    { setup_device(); }

    Arm_dma_device() : Device()
    { setup_device(); }

    int enumerate_dma_src_ids(Dma_src_feature::Dma_src_id_cb cb) const override
    {
      /*
       * src_id encoding:
       *   63-48: reserved
       *   47-32: smmu_idx
       *   31- 0: stream_id
       */
      l4_uint64_t smmu_idx = _iommu.val();
      l4_uint64_t src_id = (smmu_idx << 32) | _sid.val();

      return cb(src_id);
    }

  private:
    void setup_device()
    {
      register_property("iommu", &_iommu);
      register_property("sid", &_sid);

      property("flags")->set(-1, DF_dma_supported);

      add_feature(static_cast<Dma_src_feature *>(this));
    }

    Int_property _iommu;
    Int_property _sid;
  };
}
