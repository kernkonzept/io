/*
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *            Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "io_acpi.h"
#include "hw/pci/pci-if.h"
#include "hw/pci/pci-dev.h"
#include "hw/pci/pci-bridge.h"
#include "acpi_dmar.h"

#include <l4/cxx/minmax>

#include <vector>
#include <list>
#include <tuple>
#include <optional>
#include <limits>

#include "main.h"

namespace Hw {
namespace Acpi {

class Pci_mixin
{
protected:
  static std::optional<std::tuple<l4_uint8_t, l4_uint8_t, l4_uint8_t>>
  find_pci_bdf(l4_uint16_t segment, l4_uint8_t bus,
               Hw::Acpi::Dmar_dev_scope::Path_entry const *path_it,
               Hw::Acpi::Dmar_dev_scope::Path_entry const *path_end)
  {
    l4_uint8_t dev = 0, fn = 0;

    for (; path_it != path_end; ++path_it)
      {
        dev = path_it->dev;
        fn = path_it->func;
        Hw::Pci::Dev *pdev = Hw::Pci::find_pci_device(segment, bus, dev, fn);
        if (!pdev)
          {
            d_printf(DBG_ERR, "No PCI device found for %04x:%02x:%x.%x\n",
                     segment, bus, dev, fn);
            return std::nullopt;
          }

        if (auto *bridge = dynamic_cast<Hw::Pci::Bridge_base *>(pdev))
          bus = bridge->secondary;
        else
          // bus dev fn identifies a device.
          d_printf(DBG_DEBUG, "Found PCI Endpoint: %04x:%02x:%x.%x\n", segment,
                   bus, dev, fn);
      }

    return std::make_tuple(bus, dev, fn);
  }

};

class Vtd_iommu: Hw::Acpi::Pci_mixin
{
  struct Info
  {
    /// Range of a PCI subhierarchy with inclusive start and end.
    struct Bus_range
    {
      Bus_range(l4_uint8_t s, l4_uint8_t e) : start_bus_nr(s), end_bus_nr(e) {}
      l4_uint8_t start_bus_nr;
      l4_uint8_t end_bus_nr;
    };

    Info(l4_uint8_t idx, l4_uint64_t addr, l4_uint16_t seg, l4_uint8_t flags)
    : base_addr(addr), segment(seg), idx(idx), flags(flags)
    {}

    bool match_bus(l4_uint8_t b) const
    {
      for (Bus_range const &r : subhierarchies)
        if (r.start_bus_nr <= b && b <= r.end_bus_nr)
          return true;

      return false;
    }

    void add_subhierarchy(l4_uint8_t start, l4_uint8_t end)
    { subhierarchies.emplace_back(start, end); }

    bool pci_all() const
    { return flags & 1; }

    l4_uint64_t base_addr = 0;
    l4_uint16_t segment = 0;
    l4_uint8_t idx = 0;
    l4_uint8_t flags = 0;
    std::vector<Bus_range> subhierarchies;
  };

  struct Dev_mmu
  {
    Dev_mmu(l4_uint16_t b, l4_uint8_t d, l4_uint8_t fn, Info const &mmu)
    : bus(b), devfn(d << 3 | fn), iommu(mmu)
    {}

    l4_uint16_t bus;
    l4_uint8_t devfn;
    Info const &iommu;

    bool match(l4_uint16_t seg, l4_uint8_t b, l4_uint8_t df) const
    {
      return    seg == iommu.segment
             && b == bus
             && df == devfn;
    }

    l4_uint8_t iommu_idx() const
    { return iommu.idx; }
  };

public:
  static bool probe();

  static int get_index(l4_uint16_t segment, l4_uint8_t bus, l4_uint8_t devfn);

private:
  static void parse_drhd_entry(Hw::Acpi::Dmar_drhd const &drhd,
                               unsigned entry_num);
  static void parse_drhd_dev_scope(Hw::Acpi::Dev_scope_vect const &devs,
                                   Info &iommu);

  static std::list<Info> _iommus;
  static std::vector<Dev_mmu> _devs;
}; // class Vtd_iommu

}
}

std::list<Hw::Acpi::Vtd_iommu::Info> Hw::Acpi::Vtd_iommu::_iommus;
std::vector<Hw::Acpi::Vtd_iommu::Dev_mmu> Hw::Acpi::Vtd_iommu::_devs;

int
Hw::Acpi::Vtd_iommu::get_index(l4_uint16_t segment, l4_uint8_t bus,
                               l4_uint8_t devfn)
{
  // check explicit devices
  for (auto const &d : _devs)
    {
      if (d.match(segment, bus, devfn))
        return d.iommu_idx();
    }

  unsigned pci_all_idx = -1u;
  for (auto const &info : _iommus)
    {
      if (info.segment != segment)
        continue;

      if (info.match_bus(bus))
        return info.idx;

      if (info.pci_all())
        pci_all_idx = info.idx;
    }


  if (pci_all_idx != -1U)
    return pci_all_idx;

  return -L4_ENODEV;
}


void
Hw::Acpi::Vtd_iommu::parse_drhd_dev_scope(Hw::Acpi::Dev_scope_vect const &devs,
                                           Info &iommu)
{
  for (Hw::Acpi::Dmar_dev_scope const &dev_scope : devs)
    {
      d_printf(DBG_DEBUG,
               "Dev scope:\n\ttype: %u, length %u, enum id: %x, start bus num: "
               "0x%x, path length: %zi\n",
               dev_scope.type, dev_scope.length, dev_scope.enum_id,
               dev_scope.start_bus_nr, dev_scope.end() - dev_scope.begin());

      for (auto const &p : dev_scope)
        d_printf(DBG_DEBUG, "\tpath: %x.%x\n", p.dev, p.func);

      l4_uint16_t segment = iommu.segment;
      l4_uint8_t bus = dev_scope.start_bus_nr;
      auto path_it = dev_scope.begin();
      l4_uint8_t dev = path_it->dev;
      l4_uint8_t fn = path_it->func;
      unsigned path_length = (dev_scope.length - 6) / 2;

      switch (dev_scope.type)
        {
        case Hw::Acpi::Dmar_dev_scope::Pci_endpoint:
          if (path_length > 1)
            {
              auto bdf = find_pci_bdf(segment, bus, path_it, dev_scope.end());
              if (bdf)
                std::tie(bus, dev, fn) = bdf.value();
              else
                // error case
                break;
            }

          _devs.emplace_back(bus, dev, fn, iommu);
          break;

        case Hw::Acpi::Dmar_dev_scope::Pci_subhierarchy:
          {
            if (path_length > 1)
              d_printf(DBG_ERR,
                       "Warning: Unexpected path length of %i for PCI "
                       "sub-hierarchy.\n", path_length);

            Hw::Pci::Bridge_base *bridge =
              Hw::Pci::find_root_bridge(segment, bus);
            if (!bridge)
              {
                d_printf(DBG_DEBUG,
                         "No root bridge for segment 0x%x and start bus nr 0x%x. "
                         "Search all devices.\n",
                         segment, bus);

                bridge = dynamic_cast<Hw::Pci::Bridge_base *>(
                  Hw::Pci::find_pci_device(segment, bus, dev, fn));
              }

            if (!bridge)
              {
                d_printf(DBG_ERR, "No PCI bridge device found\n");
                break;
              }

            iommu.add_subhierarchy(bus, bridge->subordinate);
          }
          break;

        // Unhandled types:
        // case Dmar_dev_scope::Io_apic:
        // case Dmar_dev_scope::Hpet_msi:
        // case Dmar_dev_scope::Acpi_namespace_device:
        default:
          break;
        }
    }
}

void
Hw::Acpi::Vtd_iommu::parse_drhd_entry(Hw::Acpi::Dmar_drhd const &drhd,
                                      unsigned entry_num)
{
  d_printf(DBG_DEBUG,
           "DRHD[%p]: type %u, length %u, flags 0x%x, size %u, segment %u, "
           "addr 0x%llx\n",
           &drhd, drhd.type, drhd.length, drhd.flags, drhd._rsvd,
           drhd.segment, drhd.register_base);

  Info &iommu = _iommus.emplace_back(entry_num, drhd.register_base,
                                     drhd.segment, drhd.flags);

  parse_drhd_dev_scope(drhd.devs(), iommu);
}

bool Hw::Acpi::Vtd_iommu::probe()
{
  ACPI_TABLE_HEADER *tblhdr;

  if (ACPI_FAILURE(AcpiGetTable(ACPI_STRING(ACPI_SIG_DMAR), 0, &tblhdr)))
    return false;

  if (!tblhdr)
    return false;

  Acpi_dmar const *dmar_table = reinterpret_cast<Acpi_dmar const *>(tblhdr);
  unsigned count = 0;

  for (auto it = dmar_table->begin(); it != dmar_table->end(); ++it)
    {
      if (auto *drhd = it->cast<Hw::Acpi::Dmar_drhd>())
        {
          parse_drhd_entry(*drhd, count);

          // next IOMMU
          ++count;
        }
    }

  return count > 0;
}

namespace Hw {
namespace Acpi {

/**
 * Managing entity for ACPI DMAR-RMRR table.
 *
 * The class provides functionality to detect, parse and query (future) the
 * content of the RMRR table to facilitate assignment of the memory regions to
 * the corresponding devices.
 */
class Vtd_rmrr : Hw::Acpi::Pci_mixin
{
public:
  struct Devid
  {
    l4_uint32_t id = 0;
    CXX_BITFIELD_MEMBER(16, 31, segment, id);
    CXX_BITFIELD_MEMBER( 8, 15, bus, id);
    CXX_BITFIELD_MEMBER( 3,  7, dev, id);
    CXX_BITFIELD_MEMBER( 0,  2, fn, id);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, id);

    Devid(l4_uint16_t s, l4_uint8_t b, l4_uint8_t d, l4_uint8_t f)
    {
      segment() = s;
      bus() = b;
      dev() = d;
      fn() = f;
    }

    bool operator < (Devid const &other) const
    { return id < other.id; }

    bool match(Devid id) const
    { return this->id == id.id; }

  };

  struct Mem_region
  {
    l4_uint64_t start;
    l4_uint64_t end;
  };

  struct Subhierarchy
  {
    l4_uint16_t segment = 0;
    l4_uint8_t start_bus_nr = 0, end_bus_nr = 0;

    Subhierarchy(l4_uint16_t seg, l4_uint8_t s, l4_uint8_t e)
    : segment(seg), start_bus_nr(s), end_bus_nr(e)
    {}

    bool operator < (Subhierarchy const &o) const
    {
      return std::tie(segment, start_bus_nr, end_bus_nr)
             < std::tie(o.segment, o.start_bus_nr, o.end_bus_nr);
    }

    bool match(Devid id) const
    {
      if (segment != id.segment())
        return false;

      return start_bus_nr <= id.bus() && id.bus() <= end_bus_nr;
    }
  };

  Vtd_rmrr(const Acpi_dmar *dmar)
  {
    unsigned count = 0;
    for (Acpi_dmar::Iterator it = dmar->begin() ; it != dmar->end(); ++it)
      {
        Hw::Acpi::Dmar_rmrr const *entry = it->cast<Hw::Acpi::Dmar_rmrr>();
        if (!entry)
          continue;

        ++count;

        // Tell the user if the RMRR cannot be addressed by the host
        if (entry->base > std::numeric_limits<l4_addr_t>::max()
            || entry->limit > std::numeric_limits<l4_addr_t>::max())
          {
            d_printf(DBG_WARN,
                     "RMRR region [0x%llx, 0x%llx] outside phys address space.\n",
                     entry->base, entry->limit);
          }

        std::vector<Devid> ids;
        std::vector<Subhierarchy> subs;
        parse_dev_scope(entry->devs(), entry->segment, ids, subs);

        // If two devices share the same RMRR, L4Re cannot establish isolation
        // between them. Print a helpful warning to the user.
        bool warn_no_isolation = ids.size() > 1;

        // If a RMRR applies to a subhierarchy, we also cannot establish
        // isolation.
        warn_no_isolation |= subs.size() > 0;

        auto level = DBG_DEBUG;
        if (warn_no_isolation)
          {
            level = DBG_WARN;
            d_printf(DBG_WARN,
                     "This RMRR is shared by multiple devices and/or "
                     "subhierarchies. These cannot be isolated.\n");
          }

        for (Devid id : ids)
          {
            Mem_region mreg{entry->base, entry->limit};
            dev_regions[id].push_back(mreg);
            d_printf(level,
                     "PCI dev %04x::%02x::%02x.%x : RMRR [0x%llx, 0x%llx] "
                     "0x%llx\n",
                     id.segment().get(), id.bus().get(), id.dev().get(),
                     id.fn().get(),
                     mreg.start, mreg.end, mreg.end - mreg.start + 1);
          }

        for (Subhierarchy s: subs)
          {
            Mem_region mreg{entry->base, entry->limit};
            subs_regions[s].push_back(mreg);
            d_printf(level,
                     "PCI subhierarchy %04x::%02x..%02x : RMRR [0x%llx, 0x%llx] "
                     "0x%llx\n",
                     s.segment, s.start_bus_nr, s.end_bus_nr,
                     mreg.start, mreg.end, mreg.end - mreg.start + 1);
          }
      }
    d_printf(DBG_DEBUG, "Found %u RMRR entries\n", count);
  }

  static Hw::Acpi::Vtd_rmrr *get()
  {
    if (!_rmrr)
      {
        ACPI_TABLE_HEADER *tblhdr;

        if (ACPI_FAILURE(AcpiGetTable(ACPI_STRING(ACPI_SIG_DMAR), 0, &tblhdr)))
          return nullptr;

        if (!tblhdr)
          return nullptr;

        Acpi_dmar const *dmar_table = reinterpret_cast<Acpi_dmar const *>(tblhdr);
        _rmrr = new Hw::Acpi::Vtd_rmrr(dmar_table);
      }
    return _rmrr;
  }

  std::vector<Mem_region> get_blocked_regions(Devid id)
  {
    // RMRRs define memory regions that a device may use before and after the
    // IOMMU was enabled.

    std::vector<Mem_region> blocked_regions;

    {
      std::map<Devid, std::vector<Mem_region>>::iterator it;
      for (it = dev_regions.begin(); it != dev_regions.end(); ++it)
        {
          if (!it->first.match(id))
            continue;

          for (auto const &region: it->second)
            blocked_regions.push_back(region);
        }
    }
    {
      std::map<Subhierarchy, std::vector<Mem_region>>::iterator it;
      for (it = subs_regions.begin(); it != subs_regions.end(); ++it)
        {
          if (!it->first.match(id))
            continue;

          for (auto const &region: it->second)
            blocked_regions.push_back(region);
        }
    }

    return blocked_regions;
  }

private:
  static Hw::Acpi::Vtd_rmrr *_rmrr;

  std::map<Devid, std::vector<Mem_region>> dev_regions;
  std::map<Subhierarchy, std::vector<Mem_region>> subs_regions;
  std::vector<Devid> parse_dev_scope(Hw::Acpi::Dev_scope_vect const &devs,
                                     l4_uint16_t segment,
                                     std::vector<Devid> &ids,
                                     std::vector<Subhierarchy> &subs)
  {
    for (Hw::Acpi::Dmar_dev_scope const &dev_scope: devs)
      {
        l4_uint8_t bus = dev_scope.start_bus_nr;
        auto path_it = dev_scope.begin();
        l4_uint8_t dev = path_it->dev;
        l4_uint8_t fn = path_it->func;
        unsigned path_length = (dev_scope.length - 6) / 2;
        d_printf(DBG_DEBUG,
                 "Dev scope:\n\ttype: %u, length %u, enum id: %x, start bus num: "
                 "0x%x, path length: %zi\n",
                 dev_scope.type, dev_scope.length, dev_scope.enum_id,
                 dev_scope.start_bus_nr, dev_scope.end() - dev_scope.begin());

        for (auto const &p : dev_scope)
          d_printf(DBG_DEBUG, "\tpath: %x.%x\n", p.dev, p.func);

        switch (dev_scope.type)
          {
          case Hw::Acpi::Dmar_dev_scope::Pci_endpoint:
            if (path_length > 1)
              {
                auto bdf = find_pci_bdf(segment, bus, path_it, dev_scope.end());
                if (bdf)
                  std::tie(bus, dev, fn) = bdf.value();
                else
                  // error case
                  break;
              }

            ids.emplace_back(segment, bus, dev, fn);
            break;
          case Hw::Acpi::Dmar_dev_scope::Pci_subhierarchy:
            {
              if (path_length > 1)
                d_printf(DBG_ERR,
                         "Warning: Unexpected path length of %i for PCI "
                         "sub-hierarchy.\n", path_length);

              Hw::Pci::Bridge_base *bridge =
                Hw::Pci::find_root_bridge(segment, bus);
              if (!bridge)
                {
                  d_printf(DBG_DEBUG,
                           "No root bridge for segment 0x%x and start bus nr 0x%x. "
                           "Search all devices.\n",
                           segment, bus);

                  bridge = dynamic_cast<Hw::Pci::Bridge_base *>(
                    Hw::Pci::find_pci_device(segment, bus, dev, fn));
                }

              if (!bridge)
                {
                  d_printf(DBG_ERR, "No PCI bridge device found\n");
                  break;
                }

              subs.emplace_back(segment, bus, bridge->subordinate);
            }
            break;

          // Unhandled types:
          // case Dmar_dev_scope::Io_apic:
          // case Dmar_dev_scope::Hpet_msi:
          // case Dmar_dev_scope::Acpi_namespace_device:
          default:
            break;
          }
      }

    return ids;
  }
};

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

    CXX_BITFIELD_MEMBER(32, 47, iommu_idx, v);
    CXX_BITFIELD_MEMBER(18, 19, match, v);

    enum Mode
    {
      Match_requester_id = 1,
    };

    // match == Match_requester_id
    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);
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
   * supported by hardware.
   */
  int translate_dma_src(Hw::Pci::Dma_requester_id rid, l4_uint64_t *si) const override
  {
    if (!rid)
      return -L4_EINVAL;

    Vtd_dma_src_id id(0);
    id.match() = Vtd_dma_src_id::Match_requester_id;
    id.bus() = rid.bus();
    id.devfn() = rid.devfn();

    int idx = Hw::Acpi::Vtd_iommu::get_index(rid.segment(), rid.bus(),
                                             rid.devfn());
    if (idx < 0)
      {
        d_printf(DBG_WARN,
                 "Device to IOMMU assignment for %04x:%02x:%x.%x not found. "
                 "Fail.\n",
                 rid.segment().get(), rid.bus().get(), rid.dev().get(),
                 rid.fn().get());
        return idx;
      }
    else
      id.iommu_idx() = idx;

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

  int pci_enum_dma_reservations(Hw::Pci::If *, Hw::Pci::Dma_requester_id id,
                                Dma_domain_if::Resv_cb cb) const override
  {
    // report RMRRs applicable to the device
    auto devid = Hw::Acpi::Vtd_rmrr::Devid(id.segment(), id.bus(), id.dev(),
                                           id.fn());
    auto *rmrr = Hw::Acpi::Vtd_rmrr::get();
    if (rmrr == nullptr)
      return L4_EOK;
    auto regions = rmrr->get_blocked_regions(devid);

    for (auto const &region: regions)
      {
        int ret = cb(Dma_domain_if::Resv_type::Identity_fixed,
                     region.start, region.end);
        if (ret)
          return ret;
      }

    // APIC registers are treated by the interrupt remapping tables. There must
    // be no DMA mappings in this range.
    return cb(Dma_domain_if::Resv_type::Msi_window, 0xfee0'0000, 0xfeef'ffff);
  }
};

}
}

Hw::Acpi::Vtd_rmrr *Hw::Acpi::Vtd_rmrr::_rmrr;

Hw::Pci::Platform_adapter_if *
Hw::Acpi::setup_pci_platform()
{
  static Hw::Acpi::Vtd_platform_adapter vtd_adapter;
  return &vtd_adapter;
}

void
Hw::Acpi::setup_iommus()
{
  Hw::Acpi::Vtd_iommu::probe();
}
