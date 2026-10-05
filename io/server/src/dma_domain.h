#pragma once

#include "resource.h"

#include <l4/sys/task>
#include <l4/re/dma_space>
#include <l4/re/util/unique_cap>
#include <functional>
#include <map>
#include <memory>

class Dma_domain_set;
class Dma_domain_group;

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
  friend class Dma_domain_group;
private:
  static unsigned _next_free_domain;
  /**
   * Virtual DMA domain that domain belongs to.
   *
   * Usually there is one virtual DMA domain per virtual bus, and all physical
   * DMA domains that are assigned to that virtual bus belong to the same
   * virtual DMA domain. If a physical DMA domain is shared among multiple
   * virtual buses these buses also share their virtual DMA domain.
   */
  std::shared_ptr<Dma_domain_set> _v_domain;
  void add_to_set(std::shared_ptr<Dma_domain_set> const &s);

public:
  Dma_domain(Dma_domain const &) = delete;
  Dma_domain &operator = (Dma_domain const &) = delete;

  Dma_domain()
  : Resource("DMAD", Dma_domain_res, _next_free_domain, _next_free_domain)
  { ++_next_free_domain; }

  void add_to_group(Dma_domain_group *g);
};

/**
 * Gather Dma_domain's of devices on a Vbus.
 *
 * Used to assign the magic ~0U DMA domain identifier. See
 * System_bus::assign_dma_domain(). Because a device might be present on more
 * than one Vbus, there is an additional indirection through the
 * Dma_domain_group.
 */
class Dma_domain_set : public Dma_domain_if
{
  friend class Dma_domain;
  friend class Dma_domain_group;
private:
  typedef std::vector<Dma_domain *> Domains;
  typedef std::vector<Dma_domain_group *> Groups;
  Domains _domains;
  Groups _groups;

  void add_group(Dma_domain_group *g)
  { _groups.push_back(g); }

  bool rm_group(Dma_domain_group *g)
  {
    for (auto i = _groups.begin(); i != _groups.end(); ++i)
      if (*i == g)
        {
          _groups.erase(i);
          return true;
        }

    return false;
  }

  void add_domain(Dma_domain *d)
  { _domains.push_back(d); }

  bool rm_domain(Dma_domain *d)
  {
    for (auto i = _domains.begin(); i != _domains.end(); ++i)
      if (*i == d)
        {
          _domains.erase(i);
          return true;
        }

    return false;
  }

public:
  int set_managed_dma_space(std::shared_ptr<Managed_dma_space> space) override;
  void clear_managed_dma_space() override;
  int enumerate_dma_reservations(Resv_cb cb) const override;
  int get_dma_limits(l4_uint64_t *min_addr, l4_uint64_t *max_addr) const override;
};

class Dma_domain_group
{
  friend class Dma_domain;

private:
  std::shared_ptr<Dma_domain_set> _set;
  void assign(Dma_domain_group const &g)
  {
    if (_set == g._set)
      return;

    if (_set)
      _set->rm_group(this);

    _set = g._set;

    if (_set)
      _set->add_group(this);
  }

  void assign_set(std::shared_ptr<Dma_domain_set> const &s)
  {
    s->add_group(this);
    _set = s;
  }

public:
  Dma_domain_group() = default;
  Dma_domain_group(Dma_domain_group const &) = delete;
  Dma_domain_group &operator = (Dma_domain_group const &) = delete;

  ~Dma_domain_group()
  {
    if (!_set)
      return;
    _set->rm_group(this);
  }

  Dma_domain_if *operator -> () const { return _set.get(); }
  Dma_domain_if *get() const { return _set.get(); }

private:
  void merge(std::shared_ptr<Dma_domain_set> s);
};

inline void
Dma_domain::add_to_set(std::shared_ptr<Dma_domain_set> const &s)
{
  s->add_domain(this);
  _v_domain = s;
}

/**
 * Move all groups and domains of `s` into the set of this group.
 *
 * `s` is taken by value: moving its members drops their references to it, so
 * this keeps it alive until the end. Afterwards nothing refers to it anymore
 * and it is freed.
 */
inline void
Dma_domain_group::merge(std::shared_ptr<Dma_domain_set> s)
{
  for (auto g: s->_groups)
    g->assign_set(_set);

  for (auto d: s->_domains)
    d->add_to_set(_set);
}
