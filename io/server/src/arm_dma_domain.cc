#include "arm_dma_domain.h"
#include "main.h"

namespace Hw {

static void __attribute__((constructor)) setup_dma_domain_factory(void)
{
  L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
  if (iommu && iommu.validate().label())
    system_bus()->set_dma_domain_factory(new Arm_dma_domain_factory);
}

}
