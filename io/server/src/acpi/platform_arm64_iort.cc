/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <map>
#include <vector>

#include "debug.h"
#include "io_acpi.h"

namespace {

/**
 * Internal IORT translation node.
 */
struct Iort_node
{
  enum : l4_uint64_t { Translation_failed = ~(l4_uint64_t)0 };

  virtual ~Iort_node() = default;

  /**
   * Do the translation for ITS Device-IDs.
   *
   * \return Device-ID or Translation_failed on error.
   */
  virtual l4_uint64_t translate_device_id(l4_uint64_t src) const = 0;

  /**
   * Do the translation for SMMU Stream-IDs.
   *
   * \return Stream-ID or Translation_failed on error.
   */
  virtual l4_uint64_t translate_stream_id(l4_uint64_t src) const = 0;

  /**
   * Recursively parse IORT nodes.
   */
  static Iort_node *parse_mappings(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node,
                                   ACPI_TABLE_MADT *madt);
};

/**
 * An opaque Stream-/Device-ID translation node.
 *
 * Used for SMMU and root complex nodes in the IORT.
 */
class Translator : public Iort_node
{
  struct Mapping
  {
    l4_uint32_t in_base;
    l4_uint32_t num;  // Attention: number of IDs in the range minus one
    l4_uint32_t out_base;
    Iort_node *out_node;

    Mapping(l4_uint32_t in_base, l4_uint32_t num, l4_uint32_t out_base,
            Iort_node *out_node)
    : in_base(in_base), num(num), out_base(out_base), out_node(out_node)
    {}
  };

public:
  l4_uint64_t translate_device_id(l4_uint64_t src) const override
  {
    for (auto const &m : _mappings)
      {
        if (src < m.in_base || src > m.in_base + m.num)
          continue;

        src += m.out_base;
        src -= m.in_base;
        return m.out_node->translate_device_id(src);
      }

    return ~(l4_uint64_t)0;
  }

  l4_uint64_t translate_stream_id(l4_uint64_t src) const override
  {
    for (auto const &m : _mappings)
      {
        if (src < m.in_base || src > m.in_base + m.num)
          continue;

        src += m.out_base;
        src -= m.in_base;
        return m.out_node->translate_stream_id(src);
      }

    return ~(l4_uint64_t)0;
  }

  Translator(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node, ACPI_TABLE_MADT *madt)
  {
    _mappings.reserve(node->MappingCount);

    auto *mappings = ACPI_ADD_PTR(ACPI_IORT_ID_MAPPING, node,
                                  node->MappingOffset);
    for (UINT32 i = 0; i < node->MappingCount; i++)
      {
        ACPI_IORT_ID_MAPPING const &mapping = mappings[i];

        // TODO: single mappings are for SMMU or RC originating MSIs
        if (mapping.Flags & ACPI_IORT_ID_SINGLE_MAPPING)
          continue;

        auto *out_node = ACPI_ADD_PTR(ACPI_IORT_NODE, iort,
                                      mapping.OutputReference);
        auto *out_mapping = Iort_node::parse_mappings(iort, out_node, madt);
        if (!out_mapping)
          continue;

        _mappings.emplace_back(mapping.InputBase, mapping.IdCount,
                               mapping.OutputBase, out_mapping);
      }
  }

private:
  std::vector<Mapping> _mappings;
};

using Root_complex = Translator;

class Smmu : public Translator
{
public:
  Smmu(unsigned idx, ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *smmu_node,
       ACPI_TABLE_MADT *madt)
  : Translator(iort, smmu_node, madt), _idx(idx)
  {}

protected:
  l4_uint64_t translate_stream_id(l4_uint64_t src) const override
  {
    /*
     * Fiasco src encoding:
     *   63-48: reserved
     *   47-32: smmu_idx
     *   31- 0: device_id
     */
    return ((l4_uint64_t)_idx << 32) | src;
  }

private:
  unsigned _idx;
};

/**
 * ITS IORT node.
 */
struct Its : public Iort_node
{
  explicit Its(unsigned idx) : _idx(idx) {}

  l4_uint64_t translate_device_id(l4_uint64_t src) const override
  {
    /*
     * Fiasco src encoding:
     *   63-48: reserved
     *   47-32: its_idx
     *   31- 0: device_id
     */
    return ((l4_uint64_t)_idx << 32) | src;
  }

  l4_uint64_t translate_stream_id(l4_uint64_t src) const override
  {
    d_printf(DBG_ERR, "IORT: DMA translation requested for ITS (%u, 0x%llx)\n",
             _idx, src);
    return Translation_failed;
  }

private:
  unsigned _idx;
};

int
its_index(ACPI_TABLE_MADT *madt, UINT32 id)
{
  unsigned count = 0;

  for (unsigned offset = sizeof(ACPI_TABLE_MADT); offset < madt->Header.Length;)
    {
      auto *entry = ACPI_ADD_PTR(ACPI_SUBTABLE_HEADER, madt, offset);
      offset += entry->Length;
      if (entry->Type != ACPI_MADT_TYPE_GENERIC_TRANSLATOR)
        continue;

      auto *its = reinterpret_cast<ACPI_MADT_GENERIC_MSI_FRAME *>(entry);
      if (its->MsiFrameId == id)
        return count;

      count++;
    }

  d_printf(DBG_ERR, "IORT: references unknown ITS %u\n", id);
  return -1;
}

int
smmu_index(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *smmu_node)
{
  unsigned count = 0;

  for (UINT32 offset = iort->NodeOffset, i = 0;
       offset < iort->Header.Length && i < iort->NodeCount;
       i++)
    {
      auto *node = ACPI_ADD_PTR(ACPI_IORT_NODE, iort, offset);
      offset += node->Length;
      if (node->Type != ACPI_IORT_NODE_SMMU &&
          node->Type != ACPI_IORT_NODE_SMMU_V3)
        continue;

      if (node == smmu_node)
        return count;

      count++;
    }

  d_printf(DBG_ERR, "IORT: references unknown SMMU\n");
  return -1;
}

Iort_node *
Iort_node::parse_mappings(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node,
                          ACPI_TABLE_MADT *madt)
{
  switch (node->Type)
    {
    case ACPI_IORT_NODE_ITS_GROUP:
      {
        // We just use the first ITS in a group. Could be extended in the
        // future to distribute MSI sources evenly in a group.
        auto *its = ACPI_CAST_PTR(ACPI_IORT_ITS_GROUP, &node->NodeData);
        if (its->ItsCount == 0)
          {
            d_printf(DBG_ERR, "IORT: no ITS in group!\n");
            return nullptr;
          }

        int idx = its_index(madt, its->Identifiers[0]);
        if (idx < 0)
          return nullptr;

        return new Its(idx);
      }

    case ACPI_IORT_NODE_SMMU:
    case ACPI_IORT_NODE_SMMU_V3:
      {
        int idx = smmu_index(iort, node);
        if (idx < 0)
          return nullptr;

        return new Smmu(idx, iort, node, madt);
      }

    default:
      d_printf(DBG_WARN, "IORT: unexpected node type: %d\n", node->Type);
      return nullptr;
    }
}

/**
 * IORT table helper.
 *
 * Arm64 ACPI based systems describe the relationship of PCI root complexes,
 * SMMUs and ITSs by the "IO Remapping Table". See ARM DEN 0049E.
 */
class Iort : public Hw::Pci::Platform_adapter_if
{
public:
  /**
   * Parse IORT table and create translators for each PCI segment.
   *
   * Find all defined root complex nodes and traverse their dependency chain.
   */
  Iort()
  {
    ACPI_TABLE_HEADER *iort_hdr;
    ACPI_STATUS status = AcpiGetTable(ACPI_STRING(ACPI_SIG_IORT), 1, &iort_hdr);
    if (ACPI_FAILURE(status))
      return;

    // At lease on the AVA platform we see this spec violation. Linux doesn't
    // seem to care as well...
    auto *iort = reinterpret_cast<ACPI_TABLE_IORT *>(iort_hdr);
    if (iort->Header.Revision < 3)
      d_printf(DBG_WARN,
               "Firmware bug: IORT table too old: %d. Continuing anyway...\n",
               iort->Header.Revision);

    // We also need the MADT table. Fiasco uses the position of an ITS in the
    // MADT as index.
    ACPI_TABLE_HEADER *madt_hdr;
    status = AcpiGetTable(ACPI_STRING(ACPI_SIG_MADT), 1, &madt_hdr);
    if (ACPI_FAILURE(status))
      return;

    auto *madt = reinterpret_cast<ACPI_TABLE_MADT *>(madt_hdr);

    for (UINT32 offset = iort->NodeOffset, i = 0;
         offset < iort->Header.Length && i < iort->NodeCount;
         i++)
      {
        auto *node = ACPI_ADD_PTR(ACPI_IORT_NODE, iort, offset);
        offset += node->Length;
        if (node->Type != ACPI_IORT_NODE_PCI_ROOT_COMPLEX)
          continue;

        auto *rc = ACPI_CAST_PTR(ACPI_IORT_ROOT_COMPLEX, &node->NodeData);
        _pci_segments[rc->PciSegmentNumber] = new Root_complex(iort, node, madt);
      }
  }

  int translate_msi_src(::Hw::Pci::If *dev, l4_uint64_t *si) override
  {
    Root_complex const *rc = find_root_complex(dev->segment_nr());
    if (!rc)
      return -L4_ENODEV;

    // Start from a standard PCI requester ID according to Arm Base System
    // Architecture.
    l4_uint64_t src = (unsigned{dev->bus_nr()} << 8) | dev->devfn();

    /*
     * We don't care about DMA requester ID aliasing. The assumption is that
     * there are no legacy bridges on Arm64 systems. If we ever have one, we
     * will have a fun time here to figure out which is the correct alias or,
     * even worse, support multiple aliases.
     */

    src = rc->translate_device_id(src);
    if (src == Iort_node::Translation_failed)
      {
        d_printf(DBG_ERR, "IORT: untranslatable MSI source: %02x:%02x.%d\n",
                 dev->bus_nr(), dev->device_nr(), dev->function_nr());
        return -L4_ENODEV;
      }

    *si = src;
    return 0;
  }

  int translate_dma_src(::Hw::Pci::Dma_requester_id rid, l4_uint64_t *si) const override
  {
    if (!rid)
      return -L4_EINVAL;

    Root_complex const *rc = find_root_complex(rid.segment());
    if (!rc)
      return -L4_ENODEV;

    // Start from a standard PCI requester ID according to Arm Base System
    // Architecture.
    l4_uint64_t src = (unsigned{rid.bus()} << 8) | rid.devfn();

    src = rc->translate_stream_id(src);
    if (src == Iort_node::Translation_failed)
      {
        d_printf(DBG_ERR, "IORT: untranslatable DMA source: %04x:%02x:%02x.%u\n",
                 rid.segment().get(), rid.bus().get(), rid.dev().get(),
                 rid.fn().get());
        return -L4_ENODEV;
      }

    *si = src;
    return 0;
  }

  int map_msi_src(::Hw::Pci::If *dev, l4_uint64_t msi_addr_phys,
                  l4_uint64_t *msi_addr_iova) override
  {
    Dma_domain *d = dev->host()->dma_domain();
    if (!d)
      return -L4_ENODEV;

    // Without DMA space we cannot map. This might be because we have no IOMMU
    // or because the client did not yet attach the DMA space. We assume the
    // former and just leave the address as-is.
    if (!d->kern_dma_space())
      {
        *msi_addr_iova = msi_addr_phys;
        return 0;
      }

    l4_addr_t virt = res_map_iomem(msi_addr_phys, 4);
    if (!virt)
      return -L4_ENOMEM;

    auto it = _its_maps.find(msi_addr_phys);
    if (it != _its_maps.end())
      *msi_addr_iova = it->second;
    else
      {
        *msi_addr_iova = _its_next_addr + (msi_addr_phys & (L4_PAGESIZE - 1));
        _its_maps[msi_addr_phys] = *msi_addr_iova;
        _its_next_addr += L4_PAGESIZE;
      }

    int res = l4_error(
      d->kern_dma_space()->map(L4Re::This_task,
                               l4_fpage(l4_trunc_size(virt, L4_PAGESHIFT),
                                        L4_PAGESHIFT, L4_FPAGE_RW),
                               l4_trunc_size(*msi_addr_iova, L4_PAGESHIFT)));
    if (res < 0)
      d_printf(DBG_ERR,
               "error: map_msi_src failed: %d, phys=0x%llx, virt=0x%lx, iova=0x%llx\n",
               res, msi_addr_phys, virt, *msi_addr_iova);

    return res;
  }

private:
  Root_complex *find_root_complex(unsigned segment) const
  {
    auto it = _pci_segments.find(segment);
    if (it == _pci_segments.end())
      {
        d_printf(DBG_WARN, "IORT: no translation for PCI segment %d.\n",
                 segment);
        return nullptr;
      }

    return it->second;
  }

  // FIXME: this must be an address that is not in any PCI bridge window!
  // FIXME: the region must be marked as reserved in the DMA address space!
  //        Nothing else should be mapped at this addresses. Otherwise MSIs
  //        won't work.
  l4_uint64_t _its_next_addr = 0xf0000000;
  std::map<l4_uint64_t, l4_uint64_t> _its_maps;

  std::map<unsigned, Root_complex *> _pci_segments;
};

}

namespace Hw { namespace Acpi {

Hw::Pci::Platform_adapter_if *
setup_pci_platform()
{
  static Iort iort_adapter;
  return &iort_adapter;
}

} }
