/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/bid_config.h>
#include <l4/sys/capability>
#include <l4/sys/kip>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>

#include <l4/cxx/avl_tree>
#include <l4/cxx/bitmap>
#include <l4/sys/ipc.h>
#include <l4/sigma0/sigma0.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>

#include "debug.h"
#include "res.h"
#include "cfg.h"

enum
{
  Page_shift   = L4_PAGESHIFT,
  Min_rs_bits = 10,
  Min_rs_mul  = 1UL << Min_rs_bits,
  Min_rs      = Page_shift + Min_rs_bits,
};


struct Phys_region
{
  l4_addr_t phys;
  l4_addr_t size;

  bool operator < (Phys_region const &o) const noexcept
  { return phys + size - 1 < o.phys; }
  bool contains(Phys_region const &o) const noexcept
  { return o.phys >= phys && o.phys + o.size -1 <= phys + size -1; }
  Phys_region() = default;
  Phys_region(l4_addr_t phys, l4_addr_t size) : phys(phys), size(size) {}
};

struct Io_region : public Phys_region, public cxx::Avl_tree_node
{
  l4_addr_t virt;

  mutable cxx::Bitmap_base pages;
  cxx::Bitmap_base cached;

  Io_region() : virt(0), pages(0), cached(0) {}
  Io_region(Phys_region const &pr) : Phys_region(pr), virt(0), pages(0), cached(0) {}
};

struct Io_region_get_key
{
  typedef Phys_region Key_type;
  static Phys_region const &key_of(Io_region const *o) { return *o; }

};

typedef cxx::Avl_tree<Io_region, Io_region_get_key> Io_set;

static Io_set io_set;

static L4::Cap<void> sigma0;

int res_init()
{
  using L4Re::chkcap;
  sigma0 = chkcap(L4Re::Env::env()->get_cap<void>("sigma0"), "getting sigma0 cap", 0);

#if 0
  L4::Kip::Mem_desc *md = L4::Kip::Mem_desc::first(l4re_kip());
  L4::Kip::Mem_desc *mde = md + L4::Kip::Mem_desc::count(l4re_kip());
  for (; md < mde; ++md)
    {
      if (md->type() == L4::Kip::Mem_desc::Arch)
	{
	  printf("ARCH REGION %014lx-%014lx (%02x:%02x)\n",
	         md->start(), md->end(), md->type(), md->sub_type());
	  res_map_iomem(md->start(), md->size());
	}
    }
#endif
  return 0;
};

/**
 * Map physical memory range from sigma0 to the current task.
 *
 * \param phys  Physical address to map from.
 * \param virt  Virtual address to map to.
 * \param size  Size of the area to map.
 *
 * \pre `phys`, `virt`, and `size` are aligned to a power of two.
 *
 * \retval 0           Requested range mapped successfully.
 * \retval -L4_ERANGE  Input values are not page aligned.
 * \retval <0          Error during sigma0 request.
 */
static long
map_iomem_range(l4_addr_t phys, l4_addr_t virt, l4_addr_t size, bool cached)
{
  unsigned p2sz = L4_PAGESHIFT;
  long res;

  if ((phys & ~(~0UL << p2sz)) || (virt & ~(~0UL << p2sz))
      || (size & ~(~0UL << p2sz)))
    return -L4_ERANGE;

  while (size)
    {
      // Search for the largest power of two page size that fits
      // the requested region to map at once.
      while (p2sz < 22)
	{
	  unsigned n = p2sz + 1;
	  if ((phys & ~(~0UL << n)) || ((1UL << n) > size))
	    break;
	  ++p2sz;
	}

      l4_msg_regs_t *m = l4_utcb_mr_u(l4_utcb());
      l4_buf_regs_t *b = l4_utcb_br_u(l4_utcb());
      l4_msgtag_t tag = l4_msgtag(L4_PROTO_SIGMA0, 2, 0, 0);
      m->mr[0] = cached ? SIGMA0_REQ_FPAGE_IOMEM_CACHED
                        : SIGMA0_REQ_FPAGE_IOMEM;
      m->mr[1] = l4_fpage(phys, p2sz, L4_FPAGE_RWX).raw;

      b->bdr   = 0;
      b->br[0] = L4_ITEM_MAP;
      b->br[1] = l4_fpage(virt, p2sz, L4_FPAGE_RWX).raw;
      tag = l4_ipc_call(sigma0.cap(), l4_utcb(), tag, L4_IPC_NEVER);

      res = l4_error(tag);
      if (res < 0)
	return res;

      phys += 1UL << p2sz;
      virt += 1UL << p2sz;
      size -= 1UL << p2sz;

      if (!size)
	break;

      while ((1UL << p2sz) > size)
	--p2sz;
    }
  return 0;
}

/**
 * Request mapping of physical MMIO to our address space from Sigma0.
 *
 * Chose an I/O region 'iomem' which is log2-sized, log2-aligned and which
 * contains the entire region from phys..phys+size-1. Map this I/O region from
 * Sigma0 and remember that this physical region is mapped into our address
 * space (in the 'io_set' AVL tree).
 */
l4_addr_t res_map_iomem(l4_uint64_t phys, l4_uint64_t size, bool cached)
{
  if (   size > std::numeric_limits<l4_umword_t>::max()
      || phys > std::numeric_limits<l4_umword_t>::max() - size)
    {
      // This can happen on 32-bit systems where Phys_region can only handle
      // addresses up to 4GB.
      d_printf(DBG_WARN,
               "MMIO region 0x%llx/0x%llx not addressable!\n", phys, size);
      return 0;
    }

  int p2size = Min_rs;
  while ((1UL << p2size) < (size + (phys - l4_trunc_size(phys, p2size))))
    ++p2size;

  Io_region *iomem = 0;
  Phys_region r;
  r.phys = l4_trunc_page(phys);
  r.size = l4_round_page(size + phys - r.phys);

  // we need to look for proper aligned and sized regions here
  Phys_region io_reg(l4_trunc_size(phys, p2size), 1UL << p2size);

  while (1)
    {
      Io_region *reg = io_set.find_node(io_reg);
      if (!reg)
	{
	  iomem = new Io_region(io_reg);

          // start searching for virtual region at L4_PAGESIZE
#ifdef CONFIG_MMU
	  iomem->virt = L4_PAGESIZE;
	  int res = L4Re::Env::env()->rm()->reserve_area(&iomem->virt,
	      iomem->size, L4Re::Rm::F::Search_addr, p2size);
#else
	  iomem->virt = iomem->phys;
	  int res = L4Re::Env::env()->rm()->reserve_area(&iomem->virt,
	      iomem->size, L4Re::Rm::Flags(0), p2size);
#endif

	  if (res < 0)
	    {
	      delete iomem;
	      return 0;
	    }

	  unsigned bytes
	    = cxx::Bitmap_base::bit_buffer_bytes(iomem->size >> Page_shift);
          void *pages_bit_buffer = malloc(bytes);
          if (!pages_bit_buffer)
            {
              L4Re::Env::env()->rm()->free_area(iomem->virt);
              delete iomem;
              return 0;
            }
          void *cached_bit_buffer = malloc(bytes);
          if (!cached_bit_buffer)
            {
              free(pages_bit_buffer);
              L4Re::Env::env()->rm()->free_area(iomem->virt);
              delete iomem;
              return 0;
            }

	  iomem->pages = cxx::Bitmap_base(pages_bit_buffer);
	  memset(iomem->pages.bit_buffer(), 0, bytes);
	  iomem->cached = cxx::Bitmap_base(cached_bit_buffer);
	  memset(iomem->cached.bit_buffer(), 0, bytes);

	  io_set.insert(iomem);

	  d_printf(DBG_DEBUG, "new iomem region: p=%014lx v=%014lx s=%lx (bmb=%p)\n",
                   iomem->phys, iomem->virt, iomem->size,
                   iomem->pages.bit_buffer());
	  break;
	}
      else if (reg->contains(r))
	{
	  iomem = reg;
	  break;
	}
      else
	{
	  if (reg->pages.bit_buffer())
	    free(reg->pages.bit_buffer());
	  if (reg->cached.bit_buffer())
	    free(reg->cached.bit_buffer());

	  io_set.remove(*reg);
	  delete reg;
	}
    }

  l4_addr_t min = 0, max;
  bool need_map = false;
  int all_ok = 0;

  // The loop goes one page beyond the requested mapping length.
  for (unsigned i = (r.phys - iomem->phys) >> Page_shift;
       i <= ((r.size + r.phys - iomem->phys) >> Page_shift);
       ++i)
    {
      // Install required mappings as soon as we are either
      //   a) at the end of loop, or
      //   b) if a page is already mapped and we don't need to upgrade the
      //      mapping.
      if (need_map && (i == ((r.size + r.phys - iomem->phys) >> Page_shift)
	               || (iomem->pages[i] && (!cached || iomem->cached[i]))))
	{
	  max = i << Page_shift;
	  need_map = false;

	  int res = map_iomem_range(iomem->phys + min, iomem->virt + min,
	                            max - min, cached);

	  d_printf(DBG_DEBUG2, "map mem: p=%014lx v=%014lx s=%lx %s: %s(%d)\n",
	           iomem->phys + min,
                   iomem->virt + min, max - min,
                   cached ? "cached" : "uncached",
                   res < 0 ? "failed" : "done", res);

	  if (res >= 0)
	    {
	      for (unsigned x = min >> Page_shift; x < i; ++x)
	        {
		  iomem->pages.set_bit(x);
		  iomem->cached.bit(x, cached);
		}
	    }
	  else
	    all_ok = res;
	}
      else if (need_map)
        continue;
      // A new mapping region is started by either
      //   a) the first unmapped page in the range, or
      //   b) the first page that needs to be upgraded to a cached mapping.
      else if (!iomem->pages[i]
               || (iomem->pages[i] && cached && !iomem->cached[i]))
	{
	  min = i << Page_shift;
	  need_map = true;
	}
    }

  if (all_ok < 0)
    return 0;

  return iomem->virt + phys - iomem->phys;
}

#if defined(ARCH_amd64) || defined(ARCH_x86)

#include <l4/util/port_io.h>

enum
{
  Log2_num_ioports = 16,
  Num_ioports = 1U << Log2_num_ioports
};

static l4_umword_t iobitmap[Num_ioports / L4_MWORD_BITS];

static l4_umword_t get_iobit(unsigned port)
{
  return iobitmap[port / L4_MWORD_BITS] & (1UL << (port % L4_MWORD_BITS));
}

static void set_iobit(unsigned port)
{
  iobitmap[port / L4_MWORD_BITS] |= (1UL << (port % L4_MWORD_BITS));
}

int res_get_ioport(unsigned port, int size)
{
  bool map = false;
  unsigned nr_ports = 1U << size;

  if (size > Log2_num_ioports
      || port >= Num_ioports
      || port + nr_ports > Num_ioports)
    return 0;

  for (unsigned i = 0; i < nr_ports; ++i)
    if (!get_iobit(port + i))
      {
	map = true;
	break;
      }

  if (!map)
    return 0;

  int res = l4util_ioport_map(sigma0.cap(), port, size);
  if (res == 0)
    {
      for (unsigned i = 0; i < (1UL << size); ++i)
	set_iobit(port + i);
      return 0;
    }

  return res;
}

#endif
