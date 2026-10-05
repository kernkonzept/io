#include "dma_domain.h"
#include "debug.h"
#include <cassert>
#include <cstdio>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/sys/cxx/consts>
#include <l4/re/dma_space>
#include <set>

unsigned Dma_domain::_next_free_domain;
bool Dma_domain_if::_supports_remapping;

l4_ret_t
Dma_domain_if::identity_map(std::shared_ptr<Managed_dma_space> mds,
                            l4_uint64_t first, l4_uint64_t last)
{
  L4::Cap<L4::Task> myself(L4_BASE_TASK_CAP);
  l4_uint64_t phys_addr = first;
  l4_size_t size = L4::round_page(last - phys_addr + 1);
  // Create a mapping of the physical memory into IO's virtual
  // memory. This is required in order for the kernel to be able to
  // create mappings.
  l4_uint64_t virt_addr = res_map_iomem(first, size);
  if (!virt_addr)
    {
      d_printf(DBG_ERR, "DMA: Could not map identity region\n");
      return -L4_ENOMEM;
    }
  d_printf(DBG_INFO, "DMA: Identity mapping [0x%llx, 0x%llx]\n",
           phys_addr, last);
  while (size > 0)
    {
      unsigned order = L4::max_order(L4_PAGESHIFT,
                                     virt_addr,
                                     virt_addr,
                                     virt_addr + size,
                                     phys_addr);
      l4_fpage_t fp = l4_fpage(virt_addr, order, L4_FPAGE_RWX);
      auto ret = l4_error(mds->dma_task()->map(myself, fp, phys_addr));
      if (ret)
        {
          d_printf(DBG_WARN, "DMA: Identity mapping failed: %d\n",
                   ret);
          return ret;
        }
      size -= (1UL << order);
      phys_addr += (1UL << order);
      virt_addr += (1UL << order);
    }
  return L4_EOK;
}

Managed_dma_space::Managed_dma_space(L4Re::Util::Unique_cap<L4Re::Dma_space> dma_space,
                                     bool identity_mapped)
: _dma_space(std::move(dma_space)),
  _dma_task(L4Re::chkcap(L4Re::Util::make_unique_cap<L4::Task>())),
  _identity_mapped(identity_mapped)
{
  L4Re::chksys(L4Re::Env::env()->factory()
                  ->create(_dma_task.get(), L4_PROTO_DMA_SPACE));
}

Managed_dma_space::~Managed_dma_space()
{
  auto dma_mgr = L4Re::Env::env()->get_cap<L4Re::Dma_space_mgr>("dma_mgr");
  if (dma_mgr)
    {
      int ret = dma_mgr->disassociate(L4::Ipc::make_cap_rws(_dma_space.get()));
      if (ret < 0)
        d_printf(DBG_ERR, "Could not disassociate: %d\n", ret);
    }
}

l4_ret_t
Managed_dma_space::get_msi_mapping(l4_uint64_t phys, l4_uint64_t *iova)
{
  l4_uint64_t aligned_phys = L4::trunc_page(phys);
  unsigned page_offs = phys - aligned_phys;

  auto it = _msi_mappings.find(aligned_phys);
  if (it != _msi_mappings.end())
    {
      *iova = it->second + page_offs;
      return 0;
    }

  l4_addr_t virt = res_map_iomem(aligned_phys, L4_PAGESIZE);
  if (!virt)
    return -L4_ENOMEM;

  // An identity mapped DMA space has no blockings
  L4Re::Dma_space::Dma_addr dma_addr = aligned_phys;
  if (!_identity_mapped)
    {
      auto dma_mgr = L4Re::Env::env()->get_cap<L4Re::Dma_space_mgr>("dma_mgr");
      if (!dma_mgr)
        return -L4_ENOSYS;

      dma_addr = L4_PAGESIZE; // avoid 0
      l4_ret_t res =
        dma_mgr->block_area(L4::Ipc::make_cap_rws(_dma_space.get()),
                            &dma_addr, L4_PAGESIZE, -1,
                            L4Re::Dma_space_mgr::Search_addr, L4_PAGESHIFT);
      if (res < 0)
        return res;
    }

  l4_ret_t res = l4_error(_dma_task->map(L4Re::This_task,
                                         l4_fpage(virt, L4_PAGESHIFT,
                                                  L4_FPAGE_RW),
                                         dma_addr));
  if (res < 0)
    return res;

  *iova = dma_addr + page_offs;
  _msi_mappings[aligned_phys] = dma_addr;

  return 0;
}


char const *
Dma_domain_if::resv_type_to_str(Resv_type type)
{
  switch (type)
    {
    case Resv_type::Identity_fixed:      return "fixed identity mapping";
    case Resv_type::Identity_remappable: return "remapable identity mapping";
    case Resv_type::Msi_window:          return "MSI controller window";
    case Resv_type::Bridge_window:       return "P2P PCI bridge window";
    case Resv_type::Device_mmio:         return "P2P PCI device";
    }

  return "unknown";
}

int
Dma_domain_if::set_dma_space(L4Re::Util::Unique_cap<L4Re::Dma_space> dma_space,
                             Dma_domain_owner *owner)
{
  auto dma_mgr =
    L4Re::chkcap(L4Re::Env::env()->get_cap<L4Re::Dma_space_mgr>("dma_mgr"),
                 "Get DMA space manager cap from env", -L4_ENOSYS);

  bool phys = needs_phys_dma();
  if (_supports_remapping && phys)
    warn_phys_dma();

  if (!_supports_remapping || (phys && !has_iommu_dma()))
    {
      d_printf(DBG_DEBUG2, "DMA: use CPU-phys addresses for DMA\n");
      return dma_mgr->associate_phys(L4::Ipc::make_cap_rws(dma_space.get()),
                                     L4Re::Dma_space_mgr::Space_attribs::None);
    }

  /*
   * Devices not behind the IOMMU use physical DMA addresses. The devices bound
   * to the IOMMU then use the same addresses: every buffer is mapped at its
   * physical address into the DMA task (identity mapping).
   */
  if (phys)
    d_printf(DBG_DEBUG2, "DMA: devices without IOMMU: use CPU-phys addresses "
                         "for DMA, identity mapped for the IOMMU\n");
  else
    d_printf(DBG_DEBUG2, "DMA: create kern DMA space for managed DMA\n");
  auto mds = std::make_shared<Managed_dma_space>(std::move(dma_space), phys);

  // Bind the device / all devices to the Managed_dma_space. This gathers the
  // DMA address limits.
  if (int err = set_managed_dma_space(mds, owner); err < 0)
    return err;

  // Enumerate all required identity mappings and constraints. They have to be
  // reserved in the Dma_space with the exception of
  // Resv_type::Identity_remappable.

  int err = enumerate_dma_reservations(
    [this, dma_mgr, mds](Resv_type type, l4_uint64_t first, l4_uint64_t last)
    {
      first = L4::trunc_page(first); // block_area requires page alignment
      d_printf(DBG_DEBUG2, "DMA: reserve 0x%llx-0x%llx, %s\n", first, last,
               resv_type_to_str(type));

      // An identity mapped DMA space has no blockings: every region is at its
      // physical address anyway.
      if (mds->identity_mapped())
        {
          if (type == Resv_type::Identity_fixed
              || type == Resv_type::Identity_remappable)
            return identity_map(mds, first, last);
          return static_cast<l4_ret_t>(L4_EOK);
        }

      switch (type)
        {
        case Resv_type::Identity_remappable:
          d_printf(DBG_WARN,
                   "Remappable DMA reservations not fully supported yet. "
                   "Treating like identity fixed ones.\n");
          [[fallthrough]];
        case Resv_type::Identity_fixed:
          {
            // An identity mapping (e.g. an RMRR) needs to be mapped into the
            // dma_task directly, such that it is active once it is associated
            // with the dma domain. We also need to make sure Moe does not
            // allow to remap these mappings.
            l4_ret_t ret = identity_map(mds, first, last);
            if (ret)
              return ret;
          }
          [[fallthrough]];
        case Resv_type::Bridge_window:
        case Resv_type::Msi_window:
        case Resv_type::Device_mmio:
          return dma_mgr->block_area(L4::Ipc::make_cap_rws(mds->dma_space()),
                                     &first, l4_round_page(last - first + 1));
        default:
          break;
        }
      return static_cast<l4_ret_t>(L4_EOK);
    });
  if (err < 0)
    {
      d_printf(DBG_DEBUG2, "DMA: enumerate_dma_reservations failed: %d\n", err);
      clear_managed_dma_space(owner);
      return err;
    }

  l4_uint64_t min_addr, max_addr;
  err = get_dma_limits(&min_addr, &max_addr);
  if (err < 0)
    {
      d_printf(DBG_DEBUG2, "DMA: get_dma_limits failed: %d\n", err);
      clear_managed_dma_space(owner);
      return err;
    }

  if (phys)
    {
      // The physical addresses of the buffers decide, no limits possible
      d_printf(DBG_DEBUG2, "DMA: limits 0x%llx-0x%llx not enforced for "
                           "CPU-phys addresses\n", min_addr, max_addr);
    }
  else
    {
      err = dma_mgr->set_limits(L4::Ipc::make_cap_rws(mds->dma_space()),
                                min_addr, max_addr);
      if (err < 0)
        {
          d_printf(DBG_DEBUG2, "DMA: set_dma_limits failed: %d\n", err);
          clear_managed_dma_space(owner);
          return err;
        }
    }

  d_printf(DBG_DEBUG2, "DMA: associate managed DMA space (cap=%lx)\n",
           mds->dma_task().cap());
  L4Re::Dma_space_mgr::Space_attribs attribs;
  if (phys)
    attribs = L4Re::Dma_space_mgr::Identity_map;
  err = dma_mgr->associate(L4::Ipc::make_cap_rws(mds->dma_space()),
                           L4::Ipc::make_cap_rws(mds->dma_task()), attribs);
  if (err < 0)
    {
      d_printf(DBG_DEBUG2, "DMA: associate failed: %d\n", err);
      clear_managed_dma_space(owner);
    }

  return err;
}

int
Dma_domain_if::clear_dma_space(Dma_domain_owner *owner)
{
  if (!_supports_remapping)
    return L4_EOK;

  // This will unbind the device(s) from the IOMMU.
  return clear_managed_dma_space(owner) ? L4_EOK : -L4_EPERM;
}


int
Dma_domain::set_managed_dma_space(std::shared_ptr<Managed_dma_space> space,
                                  Dma_domain_owner *owner)
{
  assert(owner);
  if (_owner)
    return -L4_EBUSY;

  _managed_dma_space = space;
  _owner = owner;

  return L4_EOK;
}

bool
Dma_domain::clear_managed_dma_space(Dma_domain_owner *owner)
{
  if (!_owner)
    return true;

  if (_owner != owner)
    return false;

  _managed_dma_space = nullptr;
  _owner = nullptr;

  return true;
}


int
Dma_domain_set::set_managed_dma_space(std::shared_ptr<Managed_dma_space> space,
                                      Dma_domain_owner *owner)
{
  Domains doms = domains();
  for (auto i = doms.begin(); i != doms.end(); ++i)
    if (int err = (*i)->set_managed_dma_space(space, owner); err < 0)
      {
        // Domain i did not take the space. Only roll back the ones before it.
        while (i != doms.begin())
          (*--i)->clear_managed_dma_space(owner);
        return err;
      }

  return L4_EOK;
}

bool
Dma_domain_set::clear_managed_dma_space(Dma_domain_owner *owner)
{
  for (auto d : domains())
    d->clear_managed_dma_space(owner);

  return true;
}

int
Dma_domain_set::enumerate_dma_reservations(Resv_cb cb) const
{
  // Gather in a region set first. Usually, all devices on the Vbus report the
  // same constraints.
  struct Region
  {
    Resv_type type;
    l4_uint64_t first;
    l4_uint64_t last;

    bool operator<(Region const &o) const noexcept
    {
      if (type != o.type)
        return type < o.type;

      if (first != o.first)
        return first < o.first;

      return last < o.last;
    }
  };

  std::set<Region> reserved;
  auto gather = [&reserved](Resv_type type, l4_uint64_t first,
                            l4_uint64_t last) -> int
    {
      reserved.emplace(Region{type, first, last});
      return 0;
    };

  for (Dma_domain const *d : domains())
    {
      int err = d->enumerate_dma_reservations(gather);
      if (err < 0)
        return err;
    }

  for (Region const &r : reserved)
    if (int err = cb(r.type, r.first, r.last); err < 0)
      return err;

  return 0;
}

int
Dma_domain_set::get_dma_limits(l4_uint64_t *min_addr, l4_uint64_t *max_addr) const
{
  l4_uint64_t all_min = 0;
  l4_uint64_t all_max = -1;
  for (auto d : domains())
    {
      l4_uint64_t dom_min, dom_max;
      if (int err = d->get_dma_limits(&dom_min, &dom_max); err < 0)
        return err;

      all_min = std::max(all_min, dom_min);
      all_max = std::min(all_max, dom_max);
    }

  *min_addr = all_min;
  *max_addr = all_max;
  return 0;
}

bool
Dma_domain_set::needs_phys_dma() const
{
  for (auto d : domains())
    if (d->needs_phys_dma())
      return true;

  return false;
}

bool
Dma_domain_set::has_iommu_dma() const
{
  for (auto d : domains())
    if (d->has_iommu_dma())
      return true;

  return false;
}

void
Dma_domain_set::warn_phys_dma() const
{
  for (auto d : domains())
    d->warn_phys_dma();
}
