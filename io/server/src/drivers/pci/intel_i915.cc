/*
 * (c) 2014-2020 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "cfg.h"
#include "debug.h"
#include <pci-driver.h>

namespace {

using namespace Hw::Pci;

struct Pci_intel_i915_drv : Driver
{
  struct Opregion_header
  {
    char const sign[16];  ///< Signature: "IntelGraphicsMem"
    l4_uint32_t size;     ///< Size of OpRegion incl header in KB
    l4_uint32_t over;     ///< OpRegion version
    l4_uint32_t sver[8];  ///< System BIOS version
    l4_uint32_t vver[4];  ///< Video BIOS version
    l4_uint32_t gver[4];  ///< Graphics driver version
    l4_uint32_t mbox;     ///< Supported mailboxes
    l4_uint32_t dmod;     ///< Driver model
    l4_uint32_t pcon;     ///< Platform configuration
    l4_uint32_t dver[8];  ///< GOP/Driver version
    l4_uint32_t rm01[31]; ///< Reserved must be zero
  } __attribute__((packed));
  static_assert(sizeof(Opregion_header) == 256, "Wrong OpRegion header size.");

  /// Validate the signature of the OpRegion.
  static bool check_opregion_sig(char const *sig)
  { return 0 == memcmp(sig, "IntelGraphicsMem", 16); }

  /// Map the OpRegion and check the signature; return nullptr on failure.
  Opregion_header *map_opregion(l4_addr_t addr)
  {
    l4_addr_t base = res_map_iomem(addr, L4_PAGESIZE);
    if (!base)
      return nullptr;

    Opregion_header *hdr = reinterpret_cast<Opregion_header *>(base);

    if (!check_opregion_sig(hdr->sign))
      return nullptr;

    return hdr;
  }

  int probe(Dev *d) override
  {
    d_printf(DBG_INFO, "Found Intel i915 device\n");

    l4_uint32_t v;

    // PCI_ASLS
    d->cfg_read(0xfc, &v, Cfg_long);

    // According to the Intel IGD OpRegion specification the default value is
    // 0h which means the BIOS has not placed a memory offset into this
    // register
    if (v == 0 || v == ~0U)
      return 0;

    d_printf(DBG_DEBUG, "Found Intel i915 GPU OpRegion at %x\n", v);

    Opregion_header *hdr = map_opregion(v);
    l4_uint32_t size = 0U;
    if (hdr)
      {
        size = hdr->size;
        d_printf(DBG_DEBUG, "i915 OpRegion: size 0x%x, version 0x%x\n", size,
                 hdr->over);
      }
    else
      {
        d_printf(DBG_WARN,
                 "i915: OpRegion header invalid. Probing failed.\n");
        return 0;
      }

    unsigned flags =   Resource::Mmio_res | Resource::Mem_type_rw
                     | Resource::F_prefetchable;

    // the opregion address might not be page aligned... (0xdaf68018)
    Resource *res = new Resource(flags, l4_trunc_page(v),
                                 l4_round_page(v + size * 0x400) - 1);
    d->host()->add_resource(res);
    Device *p;
    for (p = d->host()->parent(); p && p->parent(); p = p->parent())
      ;

    if (p)
      p->request_child_resource(res, d->host());
    return 0;
  };
};

static Pci_intel_i915_drv _pci_intel_i915_drv;

struct Init
{
  Init()
  {
    _pci_intel_i915_drv.register_driver(0x8086, 0x0046);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0166);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0412);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0416);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1612);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1912);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1916);
    _pci_intel_i915_drv.register_driver(0x8086, 0x5912); // "HD Graphics 630"
    _pci_intel_i915_drv.register_driver(0x8086, 0x9bc8); // "UHD Graphics 630"
    _pci_intel_i915_drv.register_driver(0x8086, 0x3ea0); // "UHD Graphics 620"
    _pci_intel_i915_drv.register_driver(0x8086, 0x46d1); // "UHD Graphics"
  }
};

static Init init;

}
