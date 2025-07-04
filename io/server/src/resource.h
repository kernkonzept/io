/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/icu>

#include <l4/vbus/vbus_types.h>
#include <string>
#include <vector>

#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>
#include <l4/re/rm>

#include "res.h"

class Resource;
class Device;


class Resource_list : public std::vector<Resource *>
{
public:
  /**
   * \brief Find a resource by their ID.
   * \param id  The resource id (usually 4 letters ASCII, little endian
   *            encoded).
   * \return The first resource with the given ID, or null_ptr if non found.
   */
  Resource *find(l4_uint32_t id) const;

  /**
   * \brief Find a resource by their ID (using a 4-letter string).
   * \param id  The resource id (usually up to 4 letters ASCII).
   * \return The first resource with the given ID, or null_ptr if non found.
   */
  Resource *find(char const *id) const;

  template<typename PRED>
  Resource *find_if(PRED &&p) const
  {
    for (auto i: *this)
      if (p(i))
        return i;

    return nullptr;
  }
};

class Resource_space
{
public:
  virtual char const *res_type_name() const = 0;

  virtual bool request(Resource *parent, Device *pdev,
                       Resource *child, Device *cdev) = 0;
  virtual void assign(Resource *parent, Resource *child) = 0;
  virtual bool alloc(Resource *parent, Device *pdev,
                     Resource *child, Device *cdev, bool resize) = 0;
  virtual bool adjust_children(Resource *self) = 0;

protected:
  ~Resource_space() noexcept = default;
};

class Resource
{
private:
  unsigned long _f = 0;
  l4_uint32_t _id = 0;
  Resource *_p = nullptr;

public:
  virtual char const *res_type_name() const
  { return "resource"; }

  typedef l4_uint64_t Addr;
  typedef l4_uint64_t Size;

  enum Type
  {
    Invalid_res = L4VBUS_RESOURCE_INVALID,
    Irq_res     = L4VBUS_RESOURCE_IRQ,
    Mmio_res    = L4VBUS_RESOURCE_MEM,
    Io_res      = L4VBUS_RESOURCE_PORT,
    Bus_res     = L4VBUS_RESOURCE_BUS,
    Gpio_res    = L4VBUS_RESOURCE_GPIO,
    Dma_domain_res = L4VBUS_RESOURCE_DMA_DOMAIN
  };

  enum Flags : unsigned long
  {
    F_type_mask    = 0x00ff, // covers Resource::Type
    F_disabled     = 0x0100,
    F_hierarchical = 0x0200,
    F_size_aligned = 0x0800,
    F_empty        = 0x1000,
    F_rom          = 0x2000,
    F_can_resize   = 0x4000,
    F_can_move     = 0x8000,

    F_width_64bit  =   0x1'0000,
    F_relative     =   0x4'0000,
    F_internal     =   0x8'0000, ///< Internal resource not exported to vBUS

    F_prefetchable = 0x100'0000, ///< exposed on vBUS
    F_cached_mem   = 0x200'0000, ///< exposed on vBUS

    /// The upper 12-bits of the flags are exposed on the vBUS.
    F_vbus_flags_mask     = 0xfff0'0000,
    F_vbus_flags_shift    = 20,

    Mem_type_base         = 1 << F_vbus_flags_shift,
    Mem_type_r            = L4VBUS_RESOURCE_F_MEM_R * Mem_type_base,
    Mem_type_w            = L4VBUS_RESOURCE_F_MEM_W * Mem_type_base,
    Mem_type_rw           = Mem_type_r | Mem_type_w,
    Mem_type_prefetchable = L4VBUS_RESOURCE_F_MEM_PREFETCHABLE * Mem_type_base,
    Mem_type_cacheable    = L4VBUS_RESOURCE_F_MEM_CACHEABLE    * Mem_type_base,

    Irq_type_base         = 1 << F_vbus_flags_shift,
    Irq_type_mask         = L4_IRQ_F_MASK       * Irq_type_base,
    Irq_type_none         = L4_IRQ_F_NONE       * Irq_type_base,
    Irq_type_level_high   = L4_IRQ_F_LEVEL_HIGH * Irq_type_base,
    Irq_type_level_low    = L4_IRQ_F_LEVEL_LOW  * Irq_type_base,
    Irq_type_raising_edge = L4_IRQ_F_POS_EDGE   * Irq_type_base,
    Irq_type_falling_edge = L4_IRQ_F_NEG_EDGE   * Irq_type_base,
    Irq_type_both_edges   = L4_IRQ_F_BOTH_EDGE  * Irq_type_base,
  };
  static_assert(F_prefetchable == Mem_type_prefetchable);
  static_assert(F_cached_mem == Mem_type_cacheable);

  bool is_irq() const
  { return type() == Irq_res; }

  static bool is_irq_s(Resource const *r)
  { return r && r->is_irq(); }

  bool is_irq_provider() const
  { return type() == Irq_res && provided(); }

  static bool is_irq_provider_s(Resource const *r)
  { return r && r->is_irq_provider(); }

  bool irq_is_level_triggered() const
  { return (_f & Irq_type_mask) & (L4_IRQ_F_LEVEL * Irq_type_base); }

  bool irq_is_low_polarity() const
  { return (_f & Irq_type_mask) & (L4_IRQ_F_NEG   * Irq_type_base); }

  explicit Resource(unsigned long flags = 0)
  : _f(flags) {}

  Resource(unsigned long flags, Addr start, Addr end)
  : _f(flags), _s(start), _e(end), _a(end - start)
  {}

  Resource(unsigned type, unsigned long flags, Addr start, Addr end)
  : _f((type & F_type_mask) | (flags & ~F_type_mask)),
    _s(start), _e(end), _a(end - start)
  {}

  Resource(char const *id, unsigned type, Addr start, Addr end)
  : _f(type), _id(str_to_id(id)), _s(start), _e(end), _a(end - start)
  {}

  unsigned long flags() const { return _f; }
  void add_flags(unsigned long flags) { _f |= flags; }
  void del_flags(unsigned long flags) { _f &= ~flags; }
  bool hierarchical() const { return _f & F_hierarchical; }
  bool disabled() const { return _f & F_disabled; }
  bool prefetchable() const { return _f & F_prefetchable; }
  bool cached_mem() const { return _f & F_cached_mem; }
  bool empty() const { return _f & F_empty; }
  bool fixed_addr() const { return !(_f & F_can_move); }
  bool fixed_size() const { return !(_f & F_can_resize); }
  bool relative() const { return _f & F_relative; }
  bool internal() const { return _f & F_internal; }
  unsigned type() const { return _f & F_type_mask; }

  virtual bool lt_compare(Resource const *o) const
  { return end() < o->start(); }

  static l4_uint32_t str_to_id(char const *id)
  {
    l4_uint32_t res = 0;
    for (unsigned i = 0; i < 4 && id && id[i]; ++i)
      res |= static_cast<l4_uint32_t>(id[i]) << (8 * i);
    return res;
  }

  void set_id(l4_uint32_t id)
  { _id = id; }

  void set_id(char const *id)
  { _id = str_to_id(id); }

  l4_uint32_t id() const { return _id; }

  std::string id_str() const
  {
    std::string s;
    for (l4_uint32_t id = _id; id; id >>=8)
      s += id & 0xff;
    return s;
  }

public:
//private:
  void set_empty(bool empty)
  {
    if (empty)
      _f |= F_empty;
    else
      _f &= ~F_empty;
  }

public:
  void disable() { _f |= F_disabled; }
  void enable()  { _f &= ~F_disabled; }

  virtual Resource_space *provided() const { return 0; }

  void dump(char const *type, int indent) const;
  virtual void dump(int indent = 0) const;

  virtual bool compatible(Resource *consumer, bool pref = true) const
  {
    if (type() != consumer->type())
      return false;

    return prefetchable() == (consumer->prefetchable() && pref);
  }

  Resource *parent() const { return _p; }
  void parent(Resource *p) { _p = p; }

  virtual ~Resource() = default;

private:
  Addr _s = 0, _e = 0;
  l4_uint64_t _a = 0;

  void _start_end(Addr s, Addr e) { _s = s; _e = e; }

public:
  void set_empty() { _s = _e = 0; set_empty(true); }

  /**
   * Set the alignment required by the resource.
   *
   * \param a  The resource alignment, encoded as `alignment size - 1`, i.e. for
   *           example, to require a resource to be aligned on a page boundary
   *           its alignment needs to be set to `L4_PAGESIZE - 1`.
   */
  void alignment(Size a)
  {
    _a = a;
    del_flags(F_size_aligned);
  }

  bool valid() const { return flags() && _s <= _e; }

  void validate()
  {
    if (!valid())
      disable();
  }

  Addr start() const { return _s; }
  Addr end() const { return _e; }
  Size size() const { return static_cast<Size>(_e) + 1 - _s; }

  bool contains(Resource const &o) const
  { return start() <= o.start() && end() >= o.end(); }

  void start(Addr start) { _e = start + (_e - _s); _s = start; }
  void end(Addr end)
  {
    _e = end;
    set_empty(false);
  }

  void size(Size size)
  {
    _e = _s - 1 + size;
    set_empty(false);
  }

  void start_end(Addr start, Addr end)
  {
    _start_end(start, end);
    set_empty(false);
  }

  void start_size(Addr start, Size s)
  {
    _start_end(start, start - 1 + s);
    set_empty(false);
  }

  bool is_64bit() const { return flags() & F_width_64bit; }

  /**
   * Get the alignment required by the resource.
   *
   * \return The resource alignment, encoded as `alignment size - 1`, i.e. for
   *         example, a resource that is required to be aligned on a page
   *         boundary has an alignment of `L4_PAGESIZE - 1`.
   */
  l4_uint64_t alignment() const
  {
    return  flags() & F_size_aligned ? (_e - _s) : _a;
  }

  l4_uint16_t vbus_flags() const
  {
    return (flags() & F_vbus_flags_mask) >> F_vbus_flags_shift;
  }

  virtual l4_addr_t map_iomem() const
  {
    if (type() != Mmio_res)
      return 0;
    return res_map_iomem(start(), size());
  }

  virtual l4vbus_device_handle_t provider_device_handle() const
  { return ~0; }
};


class Root_resource : public Resource
{
private:
  Resource_space *_rs;

public:
  Root_resource(unsigned long flags, Resource_space *rs)
  : Resource(flags), _rs(rs) {}

  char const *res_type_name() const override
  { return "root resource"; }

  Resource_space *provided() const override { return _rs; }
  void dump(int) const override {}
};


class Mmio_data_space : public Resource
{
private:
  L4Re::Util::Unique_cap<L4Re::Dataspace> _ds_ram;

public:
  L4Re::Rm::Unique_region<l4_addr_t> _r;

  Mmio_data_space(Size size, unsigned long alloc_flags = 0)
  : Resource(Mmio_res | Mem_type_rw, 0, size - 1)
  {
    alloc_ram(size, alloc_flags);
  }

  void alloc_ram(Size size, unsigned long alloc_flags);

  l4_addr_t map_iomem() const override
  {
    return _r.get();
  }
};
