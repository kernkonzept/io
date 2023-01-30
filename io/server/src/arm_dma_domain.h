#pragma once

#include <l4/re/env>
#include <l4/sys/iommu>
#include <l4/re/error_helper>
#include <cassert>
#include "dma_domain.h"
#include "hw_device.h"

namespace Hw {

/// DMA domain for use with the ARM variants of IOMMUS, e.g. SMMU, IPMMU.
class Arm_dma_domain : public Dma_domain
{
public:
  /**
   * Creates a DMA domain for use with ARM IOMMUs.
   *
   * \param src_id  ID to identify device and IOMMU in fiasco
   */
  Arm_dma_domain(l4_uint64_t src_id)
  : _src_id(src_id)
  { _supports_remapping = true; }

  int iommu_bind(L4::Cap<L4::Iommu> iommu)
  { return l4_error(iommu->bind(_src_id, _kern_dma_space)); }

  int iommu_unbind(L4::Cap<L4::Iommu> iommu)
  { return l4_error(iommu->unbind(_src_id, _kern_dma_space)); }

  void set_managed_kern_dma_space(L4::Cap<L4::Task> s) override
  {
    Dma_domain::set_managed_kern_dma_space(s);

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    if (!iommu)
      {
        d_printf(DBG_ERR, "no IOMMU capability available\n");
        return;
      }

    int r = iommu_bind(iommu);
    if (r < 0)
      d_printf(DBG_ERR, "error: setting DMA for device: %d\n", r);
  }

  int create_managed_kern_dma_space() override
  {
    assert (!_kern_dma_space);

    auto dma = L4Re::chkcap(L4Re::Util::make_unique_cap<L4::Task>());
    L4Re::chksys(L4Re::Env::env()->factory()->create(dma.get(),
                                                     L4_PROTO_DMA_SPACE));

    set_managed_kern_dma_space(dma.release());
    return 0;
  }

  int set_dma_task(bool set, L4::Cap<L4::Task> dma_task) override
  {
    if (managed_kern_dma_space())
      return -L4_EBUSY;

    if (set && kern_dma_space())
      return -L4_EBUSY;

    if (!set && !kern_dma_space())
      return -L4_EINVAL;

    static L4::Cap<L4::Task> const &me = L4Re::This_task;
    if (!set && !me->cap_equal(kern_dma_space(), dma_task).label())
      return -L4_EINVAL;

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    if (!iommu)
      {
        d_printf(DBG_ERR, "no IOMMU capability available\n");
        return -L4_ENODEV;
      }

    if (set)
      {
        _kern_dma_space = dma_task;
        return iommu_bind(iommu);
      }
    else
      {
        if (!_kern_dma_space)
          return 0;

        int r = iommu_unbind(iommu);
        if (r < 0)
          return r;

        _kern_dma_space = L4::Cap<L4::Task>::Invalid;
      }

    return 0;
  }

private:
  l4_uint64_t _src_id;
};

class Arm_dma_domain_factory : public Dma_domain_factory
{
public:
  Arm_dma_domain *create(Hw::Device * /* bridge */, Hw::Device *dev) override
  {
    // `dev == nullptr` is a request for a DMA domain for all devices downstream
    // of `bridge`. This happens only in context of PCI and was not yet
    // observed on ARM. See also Device::dma_domain_for().
    if (!dev)
      return nullptr;

    Int_property *iommu = dynamic_cast<Int_property*>(dev->property("iommu"));
    if (!iommu)
      // not an ARM DMA device, not supported
      return nullptr;

    Int_property *sid = dynamic_cast<Int_property*>(dev->property("sid"));
    if (!sid)
      return nullptr;

    /*
     * src_id encoding:
     *   63-48: reserved
     *   47-32: smmu_idx
     *   31- 0: stream_id
     */
    l4_uint64_t smmu_idx = iommu->val();
    l4_uint64_t src_id = (smmu_idx << 32) | sid->val();
    return new Arm_dma_domain(src_id);
  }
};
}
