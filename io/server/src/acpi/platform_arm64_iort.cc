/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <functional>
#include <limits>
#include <map>
#include <set>
#include <vector>

#include <l4/sys/cxx/consts>

#include "debug.h"
#include "io_acpi.h"

namespace {

/**
 * Internal IORT translation node.
 */
struct Iort_node
{
  enum : l4_uint64_t { Translation_failed = ~(l4_uint64_t)0 };

  /**
   * Callback invoked by walk_rmr_path() for each node visited.
   *
   * \param node The ACPI IORT node visited.
   * \param id   The input ID at this node.
   */
  using Walk_rmr_cb = std::function<void(Iort_node const &node,
                                         l4_uint64_t id)>;

  explicit Iort_node(ACPI_IORT_NODE const *node) : _acpi_node(node) {}
  virtual ~Iort_node() = default;

  /**
   * The underlying ACPI IORT node. Used as a stable identity for matching
   * against RMR mapping targets.
   */
  ACPI_IORT_NODE const *acpi_node() const { return _acpi_node; }

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
   * Walk the IORT path that `src` takes when entering this node to find RMR
   * nodes.
   *
   * For each node visited (including this one), the callback is invoked
   * with the input ID at that node. The walk does not descend below SMMU
   * nodes, because RMR mappings only target the SMMU or PCI Root Complex.
   */
  virtual void walk_rmr_path(l4_uint64_t src, Walk_rmr_cb const &cb) const = 0;

  /**
   * Recursively parse IORT nodes.
   */
  static Iort_node *parse_mappings(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node,
                                   ACPI_TABLE_MADT *madt);

private:
  ACPI_IORT_NODE const *_acpi_node;
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

  void walk_rmr_path(l4_uint64_t src, Walk_rmr_cb const &cb) const override
  {
    cb(*this, src);

    for (auto const &m : _mappings)
      {
        if (src < m.in_base || src > m.in_base + m.num)
          continue;

        m.out_node->walk_rmr_path(src + m.out_base - m.in_base, cb);
        return;
      }
  }

  Translator(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node, ACPI_TABLE_MADT *madt)
  : Iort_node(node)
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

  void walk_rmr_path(l4_uint64_t src, Walk_rmr_cb const &cb) const override
  {
    // The SMMU is a terminal node for RMR matching. The SMMU's own mappings
    // describe further translation to the ITS for MSIs, which is not relevant
    // for DMA reservations.
    cb(*this, src);
  }

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
  Its(unsigned idx, ACPI_IORT_NODE const *node)
  : Iort_node(node), _idx(idx) {}

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

  void walk_rmr_path(l4_uint64_t, Walk_rmr_cb const &) const override
  {
    // An ITS is a terminal node and it should never apply to RMRs.
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

        return new Its(idx, node);
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
 * IORT Reserved Memory Range.
 *
 * Describes one or more reserved memory regions and the set of devices these
 * regions apply to. See ARM DEN 0049E.g section 2.1.7.
 */
class Rmr
{
public:
  struct Mem_region
  {
    l4_uint64_t first; ///< First byte of the region.
    l4_uint64_t last;  ///< Last byte (inclusive) of the region.
  };

  /**
   * A set of device IDs at a target node that this RMR applies to.
   */
  struct Id_range
  {
    /// Target node (SMMU or PCI Root Complex) whose input ID space the IDs
    /// below refer to.
    ACPI_IORT_NODE const *out_node;
    l4_uint32_t out_base; ///< Lowest output ID.
    l4_uint32_t num;      ///< Number of IDs minus one.
  };

  Rmr(ACPI_TABLE_IORT *iort, ACPI_IORT_NODE *node)
  {
    auto *rmr = ACPI_CAST_PTR(ACPI_IORT_RMR, &node->NodeData);
    _remap_permitted = rmr->Flags & ACPI_IORT_RMR_REMAP_PERMITTED;

    if (   !node->MappingOffset || !node->MappingCount
        || !rmr->RmrOffset      || !rmr->RmrCount)
      {
        d_printf(DBG_WARN, "IORT: Invalid RMR node! Skipping...\n");
        return;
      }

    auto *descs = ACPI_ADD_PTR(ACPI_IORT_RMR_DESC, node, rmr->RmrOffset);
    _regions.reserve(rmr->RmrCount);
    for (UINT32 i = 0; i < rmr->RmrCount; i++)
      {
        if (descs[i].Length == 0)
          continue;

        // Warn if the region cannot be addressed by the host.
        l4_uint64_t base = descs[i].BaseAddress;
        l4_uint64_t length = descs[i].Length;

        // According to the IORT, the region must be 64KiB aligned. Just warn,
        // we can cope with unaligned addresses.
        if (   L4::trunc_order(base, 16) != base
            || L4::trunc_order(length, 16) != length)
          d_printf(DBG_WARN,
                   "IORT: Firmware bug! RMR region not aligned:  [0x%llx, 0x%llx]\n",
                   base, base + length - 1);

        if (base > std::numeric_limits<l4_addr_t>::max()
            || std::numeric_limits<l4_addr_t>::max() - base < length - 1)
          d_printf(DBG_WARN,
                   "IORT: RMR region [0x%llx, 0x%llx] outside phys address space.\n",
                   base, base + length - 1);

        _regions.push_back({base, base + length - 1});
      }

    auto *mappings = ACPI_ADD_PTR(ACPI_IORT_ID_MAPPING, node,
                                  node->MappingOffset);
    _id_ranges.reserve(node->MappingCount);
    for (UINT32 i = 0; i < node->MappingCount; i++)
      {
        ACPI_IORT_ID_MAPPING const &mapping = mappings[i];
        if (!(mapping.Flags & ACPI_IORT_ID_SINGLE_MAPPING))
          d_printf(DBG_WARN,
                   "IORT: Firmware bug! Expected single mapping in RMR.\n");

        auto *out_node = ACPI_ADD_PTR(ACPI_IORT_NODE, iort,
                                      mapping.OutputReference);
        _id_ranges.push_back({out_node, mapping.OutputBase, mapping.IdCount});
      }
  }

  bool matches(Iort_node const &node, l4_uint64_t id) const
  {
    for (Id_range const &range : _id_ranges)
      {
        if (range.out_node != node.acpi_node())
          continue;
        if (id < range.out_base || id > range.out_base + range.num)
          continue;

        return true;
      }

    return false;
  }

  bool remap_permitted() const { return _remap_permitted; }
  std::vector<Mem_region> const &regions() const { return _regions; }

private:
  bool _remap_permitted = false;
  std::vector<Mem_region> _regions;
  std::vector<Id_range> _id_ranges;
};

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
        switch (node->Type)
          {
          case ACPI_IORT_NODE_PCI_ROOT_COMPLEX:
            {
              auto *rc = ACPI_CAST_PTR(ACPI_IORT_ROOT_COMPLEX, &node->NodeData);
              _pci_segments[rc->PciSegmentNumber] =
                new Root_complex(iort, node, madt);
              break;
            }
          case ACPI_IORT_NODE_RMR:
            _rmrs.emplace_back(iort, node);
            break;
          default:
            break;
          }
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

    // Without IOMMU we must return the actual physical address.
    if (!d->supports_remapping())
      {
        *msi_addr_iova = msi_addr_phys;
        return 0;
      }

    std::shared_ptr<Managed_dma_space> mds = d->managed_dma_space();
    if (!mds)
      // Apparently, the client did not yet attach the DMA space...
      return -L4_ENODEV;

    int res = mds->get_msi_mapping(msi_addr_phys, msi_addr_iova);
    if (res < 0)
      d_printf(DBG_ERR, "error: get_msi_mapping() failed: %d, phys=0x%llx\n",
               res, msi_addr_phys);

    return res;
  }

  int pci_enum_dma_reservations(Hw::Pci::If *, Hw::Pci::Dma_requester_id rid,
                                Dma_domain_if::Resv_cb cb) const override
  {
    if (!rid || _rmrs.empty())
      return L4_EOK;

    Root_complex const *rc = find_root_complex(rid.segment());
    if (!rc)
      return L4_EOK;

    l4_uint64_t src = (unsigned{rid.bus()} << 8) | rid.devfn();

    // Determine which RMRs apply by walking the device's IORT path. An RMR
    // applies to the device if any of its ID range mappings target a node
    // visited by the device's path and the device's input ID at that node
    // falls into the RMR's output ID range.
    std::set<Rmr const *> matched;
    auto node_cb = [this, &matched](Iort_node const &node, l4_uint64_t id)
      {
        for (Rmr const &rmr : _rmrs)
          {
            if (matched.count(&rmr))
              continue;

            if (rmr.matches(node, id))
              matched.insert(&rmr);
          }
      };
    rc->walk_rmr_path(src, node_cb);

    for (Rmr const *rmr : matched)
      {
        auto type = rmr->remap_permitted()
                    ? Dma_domain_if::Resv_type::Identity_remappable
                    : Dma_domain_if::Resv_type::Identity_fixed;
        for (Rmr::Mem_region const &region : rmr->regions())
          {
            d_printf(DBG_DEBUG,
                     "IORT: %04x:%02x:%02x.%u : RMR [0x%llx, 0x%llx]\n",
                     rid.segment().get(), rid.bus().get(), rid.dev().get(),
                     rid.fn().get(), region.first, region.last);
            if (int ret = cb(type, region.first, region.last); ret < 0)
              return ret;
          }
      }

    return L4_EOK;
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

  std::map<unsigned, Root_complex *> _pci_segments;
  std::vector<Rmr> _rmrs;
};

}

Hw::Pci::Platform_adapter_if *
Hw::Acpi::setup_pci_platform()
{
  static Iort iort_adapter;
  return &iort_adapter;
}
