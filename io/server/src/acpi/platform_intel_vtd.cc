/*
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "io_acpi.h"
#include "pci-if.h"

namespace Hw {
namespace Acpi {

class Vtd_platform_adapter : public Hw::Pci::Platform_adapter_if
{
  /**
   * Intel VT-d interrupt remapping table entry format.
   *
   * This format is used by Fiasco as `source` parameter to the system Icu
   * `msi_info()` call on x86. It it programmed directly into the IOMMU.
   */
  struct Vtd_irte_src_id
  {
    l4_uint64_t v = 0;
    Vtd_irte_src_id(l4_uint64_t v) : v(v) {}

    CXX_BITFIELD_MEMBER(18, 19, svt, v);

    enum Source_validation_type
    {
      Svt_none         = 0,
      Svt_requester_id = 1,
      Svt_bus_range    = 2,
    };

    CXX_BITFIELD_MEMBER(16, 17, sq, v);

    // svt == Svt_requester_id
    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);

    // svt == Svt_bus_range
    CXX_BITFIELD_MEMBER( 8, 15, start_bus, v);
    CXX_BITFIELD_MEMBER( 0,  7, end_bus, v);
  };

  /**
   * Intel VT-d dma source id.
   *
   * This format is used by Fiasco as `src_id` parameter to Iommu::bind() on
   * x86.
   */
  struct Vtd_dma_src_id
  {
    l4_uint64_t v = 0;
    Vtd_dma_src_id(l4_uint64_t v) : v(v) {}

    CXX_BITFIELD_MEMBER(18, 19, match, v);

    enum Mode
    {
      Match_requester_id = 1,
      Match_bus          = 2,
    };

    // match == Match_requester_id
    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);

    // match == Match_bus
    CXX_BITFIELD_MEMBER( 8, 15, whole_bus, v);
  };

public:
  /**
   * Translate PCI device into an opaque MSI source-ID.
   *
   * We have to deal with the additional problem of DMA aliases here.
   * Fortunately the Intel VT-d interrupt remapping entry allows for enough
   * flexibility to match common alias conditions.
   */
  int translate_msi_src(Hw::Pci::If *dev, l4_uint64_t *si) override
  {
    // By default the exact requester ID is used
    Vtd_irte_src_id id(0);
    id.svt() = Vtd_irte_src_id::Svt_requester_id;
    id.sq() = dev->phantomfn_bits();
    id.bus() = dev->bus_nr();
    id.devfn() = dev->devfn();

    // Walk up the bus hierarchy to see if there are aliasing bridges. We don't
    // need to care about the segment number because that will always stay the
    // same in a bus hierarchy.
    for (Hw::Pci::Bridge_if *bridge = dev->bridge();
         bridge;
         bridge = bridge->parent_bridge())
      {
        if (auto alias = bridge->dma_alias())
          {
            if (alias.is_rewrite())
              {
                // legacy PCI bridge
                id.svt() = Vtd_irte_src_id::Svt_requester_id;
                id.sq() = 0;
                id.bus() = alias.bus();
                id.devfn() = alias.devfn();
              }
            else if (alias.is_alias() && alias.bus() == id.bus())
              {
                // PCIe-to-PCI(-X) bridge
                id.svt() = Vtd_irte_src_id::Svt_bus_range;
                id.start_bus() = alias.bus();
                id.end_bus() = alias.bus();
              }
            else
              {
                d_printf(DBG_WARN, "Cannot handle DMA alias: %s: 0x%x\n",
                         alias.as_cstr(), alias.addr);
                return -L4_EINVAL;
              }
          }
      }

    *si = id.v;
    return 0;
  }

  /**
   * Translate DMA source ID.
   *
   * We always match the exact requester ID as this is the only thing that is
   * supported by hardware. Matching the bus (Match_bus) is a Fiasco extension.
   */
  int translate_dma_src(Hw::Pci::Dma_requester_id rid, l4_uint64_t *si) const override
  {
    if (!rid)
      return -L4_EINVAL;

    Vtd_dma_src_id id(0);
    id.match() = Vtd_dma_src_id::Match_requester_id;
    id.bus() = rid.bus();
    id.devfn() = rid.devfn();
    *si = id.v;

    return 0;
  }

  int map_msi_src(::Hw::Pci::If *, l4_uint64_t msi_addr_phys,
                  l4_uint64_t *msi_addr_iova) override
  {
    // MSI controller address is handled specially on Intel VT-d and requires
    // no mapping in the device IOVA.
    *msi_addr_iova = msi_addr_phys;
    return 0;
  }
};

Hw::Pci::Platform_adapter_if *
setup_pci_platform()
{
  static Vtd_platform_adapter vtd_adapter;
  return &vtd_adapter;
}

}
}
