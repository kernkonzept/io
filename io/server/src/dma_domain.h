#pragma once

#include "resource.h"

#include <l4/sys/task>
#include <l4/re/dma_space>
#include <l4/re/util/unique_cap>
#include <functional>
#include <map>
#include <memory>
#include <vector>

/**
 * A Dma_space and its associated kernel DMA task.
 */
class Managed_dma_space
{
public:
  Managed_dma_space(L4Re::Util::Unique_cap<L4Re::Dma_space> dma_space);
  ~Managed_dma_space();

  L4::Cap<L4Re::Dma_space> dma_space() const { return _dma_space.get(); }
  L4::Cap<L4::Task> dma_task() const { return _dma_task.get(); }

  /**
   * Get (or create) mapping of MSI controller in this Dma_space.
   *
   * \param      phys  The physical address of the MSI controller
   * \param[out] iova  DMA virtual address of the mapping
   */
  l4_ret_t get_msi_mapping(l4_uint64_t phys, l4_uint64_t *iova);

private:
  L4Re::Util::Unique_cap<L4Re::Dma_space> _dma_space;
  L4Re::Util::Unique_cap<L4::Task> _dma_task;
  std::map<l4_uint64_t, l4_uint64_t> _msi_mappings;
};

class Dma_domain_if
{
protected:
  std::shared_ptr<Managed_dma_space> _managed_dma_space;
  static bool _supports_remapping;

public:
  /**
   * Type of reserved memory region.
   */
  enum class Resv_type
  {
    Identity_fixed,       ///< Fixed, identity-mapped region.
    Identity_remappable,  ///< Identity-mapped region that may be remapped.
    Msi_window,           ///< Window to MSI controller
    Bridge_window,        ///< A bridge window that may route P2P traffic
    Device_mmio,          ///< A device, reachable through P2P traffic
  };

  static char const *resv_type_to_str(Resv_type type);

  /**
   * Callback receiving reserved memory regions.
   *
   * \param type  The region type
   * \param first The first byte in the region
   * \param last  The last byte in the region (inclusive)
   *
   * \retval  >=0 Success
   * \retval  <0  Failure. Stops further enumeration.
   */
  using Resv_cb = std::function<int(Resv_type type, l4_uint64_t first,
                                    l4_uint64_t last)>;

  std::shared_ptr<Managed_dma_space> managed_dma_space() const
  { return _managed_dma_space; }

  static bool supports_remapping()
  { return _supports_remapping; }

  int set_dma_space(L4Re::Util::Unique_cap<L4Re::Dma_space> dma_space);
  int clear_dma_space();

  virtual int set_managed_dma_space(std::shared_ptr<Managed_dma_space> space);
  virtual void clear_managed_dma_space();

  virtual int enumerate_dma_reservations(Resv_cb cb) const = 0;
  virtual int get_dma_limits(l4_uint64_t *min_addr, l4_uint64_t *max_addr) const = 0;

  virtual ~Dma_domain_if() = default;

private:
  l4_ret_t identity_map(std::shared_ptr<Managed_dma_space> mds,
                        l4_uint64_t first, l4_uint64_t last);
};

class Dma_domain : public Resource, public Dma_domain_if
{
private:
  static unsigned _next_free_domain;

public:
  Dma_domain(Dma_domain const &) = delete;
  Dma_domain &operator = (Dma_domain const &) = delete;

  Dma_domain()
  : Resource("DMAD", Dma_domain_res, _next_free_domain, _next_free_domain)
  { ++_next_free_domain; }
};

/**
 * Several Dma_domain's that are bound to a common DMA space.
 *
 * Used for the magic ~0U DMA domain identifier of a Vbus, which stands for
 * all DMA domains on that Vbus. See System_bus::assign_dma_domain(). The
 * derived class provides the domains.
 *
 * A Dma_domain can only be bound to one DMA space. If it is already bound
 * to another one, e.g. because its device is also on another Vbus, binding
 * the set fails.
 */
class Dma_domain_set : public Dma_domain_if
{
public:
  typedef std::vector<Dma_domain *> Domains;

  /**
   * Get the domains of the set.
   *
   * The domains are queried on each operation, so the set may change between
   * binding and unbinding. Unbinding only affects domains that are bound to
   * the DMA space of the set.
   */
  virtual Domains domains() const = 0;

  int set_managed_dma_space(std::shared_ptr<Managed_dma_space> space) override;
  void clear_managed_dma_space() override;
  int enumerate_dma_reservations(Resv_cb cb) const override;
  int get_dma_limits(l4_uint64_t *min_addr, l4_uint64_t *max_addr) const override;
};
