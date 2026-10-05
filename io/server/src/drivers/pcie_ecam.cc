/*
 * Copyright (C) 2019-2025 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * Driver for the QEMU ECAM PCI controller emulation.
 *
 * The Linux device tree for that device looks like this:
 *
 * \code{.dts}
 *  pcie@10000000 {
 *      interrupt-map-mask = <0x1800 0x00 0x00 0x07>;
 *      interrupt-map = <  0x00 0x00 0x00 0x01 0x8001 0x00 0x00 0x00 0x03 0x04
 *                         0x00 0x00 0x00 0x02 0x8001 0x00 0x00 0x00 0x04 0x04
 *                         0x00 0x00 0x00 0x03 0x8001 0x00 0x00 0x00 0x05 0x04
 *                         0x00 0x00 0x00 0x04 0x8001 0x00 0x00 0x00 0x06 0x04
 *                        0x800 0x00 0x00 0x01 0x8001 0x00 0x00 0x00 0x04 0x04
 *                        0x800 0x00 0x00 0x02 0x8001 0x00 0x00 0x00 0x05 0x04
 *                        0x800 0x00 0x00 0x03 0x8001 0x00 0x00 0x00 0x06 0x04
 *                        0x800 0x00 0x00 0x04 0x8001 0x00 0x00 0x00 0x03 0x04
 *                       0x1000 0x00 0x00 0x01 0x8001 0x00 0x00 0x00 0x05 0x04
 *                       0x1000 0x00 0x00 0x02 0x8001 0x00 0x00 0x00 0x06 0x04
 *                       0x1000 0x00 0x00 0x03 0x8001 0x00 0x00 0x00 0x03 0x04
 *                       0x1000 0x00 0x00 0x04 0x8001 0x00 0x00 0x00 0x04 0x04
 *                       0x1800 0x00 0x00 0x01 0x8001 0x00 0x00 0x00 0x06 0x04
 *                       0x1800 0x00 0x00 0x02 0x8001 0x00 0x00 0x00 0x03 0x04
 *                       0x1800 0x00 0x00 0x03 0x8001 0x00 0x00 0x00 0x04 0x04
 *                       0x1800 0x00 0x00 0x04 0x8001 0x00 0x00 0x00 0x05 0x04>;
 *      #interrupt-cells = <0x01>;
 *      ranges = <0x1000000 0x00 0x00000000 0x00 0x3eff0000 0x00 0x00010000
 *                0x2000000 0x00 0x10000000 0x00 0x10000000 0x00 0x2eff0000
 *                0x3000000 0x80 0x00000000 0x80 0x00000000 0x80 0x00000000>;
 *      reg = <0x40 0x10000000 0x00 0x10000000>;
 *      msi-parent = <0x8002>;
 *      dma-coherent;
 *      bus-range = <0x00 0xff>;
 *      linux,pci-domain = <0x00>;
 *      #size-cells = <0x02>;
 *      #address-cells = <0x03>;
 *      device_type = "pci";
 *      compatible = "pci-host-ecam-generic";
 *      iommu-map = <0x0000 &iommu0 0x4000 0x8000
 *                   0x8000 &iommu1 0x8000 0x8000>;
 *      iommu-map-mask = <0xfff8>;
 *      msi-map = <0x0000 &msi_controller0 0x0000 0x8000,
 *                 0x8000 &msi_controller1 0xa000 0x8000,>;
 *      msi-map-mask = <0xfff8>;
 *  };
 * \endcode
 *
 * The ranges property can be interpreted as follows, see
 * https://elinux.org/Device_Tree_Usage#PCI_Host_Bridge
 *
 * \verbatim
 *   0x3000000 0x80 0x00000000  0x80 0x00000000  0x80 0x00000000>;
 *   phys.hi+phys.mid+phys.low  region base      region size
 *
 *   phys.hi: npt000ss bbbbbbbb dddddfff rrrrrrrr
 *     n: relocation relocatable region flag (not relevant here)
 *     p: prefetchable (cacheable) region flag
 *     t: aliased address flags (not relevant here)
 *     s: space code (0=cfg space, 1=I/O space, 2=32-bit MMIO, 3=64-bit MMIO)
 *     b: PCI bus number
 *     d: PCI device number
 *     f: PCI function number
 *     r: register number (not relevant here)
 *   phys.mid+phys.log: PCI child address
 * \endverbatim
 *
 * This entry should be converted into the following io.cfg code:
 *
 * \code{.lua}
 *  pciec0 = Io.Hw.Ecam_pcie_bridge(function()
 *    Property.regs_base    =   0x10000000
 *    Property.regs_size    =   0x2eff0000
 *    Property.cfg_base     = 0x4010000000
 *    Property.cfg_size     = 0x0010000000 -- 256 buses x 256 devs x 4KB
 *    Property.ioport_base  =   0x3eff0000
 *    Property.ioport_size  =      0x10000 -- 64KB (for port I/O access)
 *    Property.mmio_base    =   0x10000000
 *    Property.mmio_size    =   0x2eff0000 --~750MB (for 32-bit MMIO access)
 *    Property.mmio_base_64 = 0x8000000000
 *    Property.mmio_size_64 = 0x8000000000 -- 512GB (for 64-bit MMIO access)
 *    Property.int_a        = 32 + 3
 *    Property.int_b        = 32 + 4
 *    Property.int_c        = 32 + 5
 *    Property.int_d        = 32 + 6
 *
 *    -- The iommu_map table holds 4 values for each mapping range. The
 *    -- individual elements are:
 *    --   1. Device ID input base
 *    --   2. Reference to a Hw.Iommu device
 *    --   3. Device ID output base
 *    --   4. Number of Device IDs in mapping range
 *    -- We always assume #iommu-cells = 1.
 *    -- In case the iommu node in your Devicetree has zero cells, just pass a
 *    -- placeholder of 0 as Device ID output base.
 *    -- In case the iommu node in your Devicetree has two cells, combine the
 *    -- two 32-bit cells into one 64-bit Device ID output base.
 *    Property.iommu_map    = { 0x0000, iommu0, 0x4000, 0x8000,
 *                              0x8000, iommu1, 0x8000, 0x8000 }
 *    Property.iommu_map_mask = 0xfff8
 *
 *    -- The msi_map table has the same structure as iommu_map.
 *    -- We always assume #msi-cells = 1.
 *    -- In case the iommu node in your Devicetree has zero cells, just pass a
 *    -- placeholder of 0 as Device ID output base.
 *    Property.msi_map      = { 0x0000, msi_controller0, 0x0000, 0x8000,
 *                              0x8000, msi_controller1, 0xa000, 0x8000 }
 *    Property.msi_map_mask = 0xfff8
 *  end)
 * \endcode
 */

#include <l4/drivers/hw_mmio_register_block>

#include "hw_device.h"
#include <pci-root.h>
#include "iommu.h"
#include "msi_controller.h"
#include "resource_provider.h"

namespace {

template<typename TRANSLATOR>
class Map_property : public Property
{
public:
  enum : l4_uint64_t { Translation_failed = ~l4_uint64_t{0} };

  int set(int, std::string const &) override { return -EINVAL; }

  // TODO: Maybe check that all mappings were initialized properly (count set()
  //       calls, ensure k increments by 1 between each call and the number of
  //       calls is a multiple of Num_cells).

  int set(int k, l4_int64_t val) override
  {
    if (k < 1 || val < 0)
      return -EINVAL;

    // lua tables start at index 1
    unsigned index = k - 1;
    unsigned mapping_index = index / Num_cells;
    if (mapping_index >= _mappings.size())
      _mappings.resize(mapping_index + 1);
    auto &mapping = _mappings[mapping_index];

    switch (index % Num_cells)
      {
      case 0: mapping.in_base = val; break;
      case 1: return -EINVAL;
      case 2: mapping.out_base = val; break;
      case 3: mapping.length = val; break;
      }

    return 0;
  }
  int set(int k, Generic_device *val) override
  {
    if (k < 1)
      return -EINVAL;

    // lua tables start at index 1
    unsigned index = k - 1;
    if ((index % Num_cells) != 1)
      return -EINVAL;

    TRANSLATOR *translator = dynamic_cast<TRANSLATOR *>(val);
    if (!translator)
      return -EINVAL;

    unsigned mapping_index = index / Num_cells;
    if (mapping_index >= _mappings.size())
      return -EINVAL;
    _mappings[mapping_index].translator = translator;

    return 0;
  }

  int set(int, Resource *) override { return -EINVAL; }

  l4_uint64_t translater_requester_id(l4_uint64_t requester_id) const
  {
    for (auto const &m : _mappings)
      {
        if (requester_id < m.in_base || requester_id >= m.in_base + m.length)
          continue;

        l4_uint64_t device_id = requester_id + m.out_base - m.in_base;
        return m.translator->translate_device_id(device_id);
      }

    return Translation_failed;
  }

  bool present() const { return _mappings.size() > 0; }

private:
  static constexpr unsigned Num_cells = 4;

  // Maps any device ID in the interval [in_base, in_base + length] to a source
  // ID via `translator->translate_device_id(device_id - in_base + out_base)`.
  struct Mapping
  {
    l4_uint32_t in_base;
    TRANSLATOR *translator;
    l4_uint64_t out_base;
    l4_uint32_t length; // number of IDs in the range
  };

  std::vector<Mapping> _mappings;
};

using Iommu_map_property = Map_property<Hw::Iommu>;
using Msi_map_property = Map_property<Hw::Msi_controller>;

class Ecam_pcie_bridge
: public Hw::Device,
  public Hw::Pci::Root_bridge
{
public:
  explicit Ecam_pcie_bridge(int segment = 0, unsigned bus_nr = 0)
  : Hw::Device(0xffee0000), // just don't use the default 0xffffffff ID
    Hw::Pci::Root_bridge(segment, bus_nr, this, nullptr)
  {
    // Use a default name as long as no name was set with the Lua script.
    set_name_if_empty("pcie_ecam");

    // the set of mandatory properties
    register_property("regs_base", &_regs_base);        // mandatory
    register_property("regs_size", &_regs_size);        // mandatory
    register_property("cfg_base", &_cfg_base);          // mandatory
    register_property("cfg_size", &_cfg_size);          // mandatory
    register_property("ioport_base", &_ioport_base);    // optional
    register_property("ioport_size", &_ioport_size);    // optional
    register_property("mmio_base", &_mmio_base);        // mandatory
    register_property("mmio_size", &_mmio_size);        // mandatory
    register_property("mmio_base_64", &_mmio_base_64);  // optional
    register_property("mmio_size_64", &_mmio_size_64);  // optional
    register_property("int_a", &_int_map[0]);           // optional
    register_property("int_b", &_int_map[1]);           // optional
    register_property("int_c", &_int_map[2]);           // optional
    register_property("int_d", &_int_map[3]);           // optional
    register_property("iommu_map", &_iommu_map);        // optional, no DMA mapping if not present
    register_property("iommu_map_mask", &_iommu_map_mask); // optional
    register_property("msi_map", &_msi_map);            // optional, no MSI mapping if not present
    register_property("msi_map_mask", &_msi_map_mask);  // optional
  }

  typedef Hw::Pci::Cfg_addr Cfg_addr;
  typedef Hw::Pci::Cfg_width Cfg_width;

  void init() override;

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

  int int_map(int i) const { return _int_map[i]; }

  int pci_enum_dma_reservations(Hw::Pci::If *, Hw::Pci::Dma_requester_id,
                                Dma_domain_if::Resv_cb) const override
  {
    return 0;
  }

  int translate_msi_src(Hw::Pci::If *dev, l4_uint64_t *si) override
  {
    if (!_msi_map.present())
      return -L4_ENODEV;

    // Start from a standard PCI requester ID.
    l4_uint64_t src = (unsigned{dev->bus_nr()} << 8) | dev->devfn();

    /*
     * We don't care about DMA requester ID aliasing. The assumption is that
     * there are no legacy bridges. If we ever have one, we will have a fun time
     * here to figure out which is the correct alias or, even worse, support
     * multiple aliases.
     */

    src = _msi_map.translater_requester_id(src & _msi_map_mask);
    if (src == Msi_map_property::Translation_failed)
      {
        d_printf(DBG_ERR, "%s: untranslatable MSI source: %02x:%02x.%d\n",
                 name(), dev->bus_nr(), dev->device_nr(), dev->function_nr());
        return -L4_ENODEV;
      }

    *si = src;
    return 0;
  }

  int translate_dma_src(Hw::Pci::Dma_requester_id rid, l4_uint64_t *si) const override
  {
    if (!_iommu_map.present())
      {
        d_printf(DBG_ERR,
                 "%s: cannot translate DMA source %04x:%02x:%02x.%u: "
                 "no iommu_map provided!\n",
                 name(), rid.segment().get(), rid.bus().get(), rid.dev().get(),
                 rid.fn().get());
        return -L4_ENODEV;
      }

    // Start from a standard PCI requester ID according.
    l4_uint64_t src = (unsigned{rid.bus()} << 8) | rid.devfn();

    src = _iommu_map.translater_requester_id(src & _iommu_map_mask);
    if (src == Iommu_map_property::Translation_failed)
      {
        d_printf(DBG_ERR, "%s: untranslatable DMA source: %04x:%02x:%02x.%u\n",
                 name(), rid.segment().get(), rid.bus().get(), rid.dev().get(),
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
      d_printf(DBG_ERR, "%s: get_msi_mapping() failed: %d, phys=0x%llx\n",
               name(), res, msi_addr_phys);

    return res;
  }

private:
  int host_init();

  Int_property _regs_base{~0};
  Int_property _regs_size{~0};
  Int_property _cfg_base{~0};
  Int_property _cfg_size{~0};
  Int_property _ioport_base{~0};
  Int_property _ioport_size{~0};
  Int_property _mmio_base{~0};
  Int_property _mmio_size{~0};
  Int_property _mmio_base_64{~0};
  Int_property _mmio_size_64{~0};
  Int_property _int_map[4];
  // Maps a PCI requester ID to an IOMMU and source ID identifying the device at
  // the IOMMU.
  Iommu_map_property _iommu_map;
  // Mask to apply to PCI requester ID before mapping via _iommu_map.
  // (maximum and default value is 0xffff)
  Int_property _iommu_map_mask = 0xffffU;
  // Maps a PCI requester ID to an MSI controller and source ID identifying the
  // device at the MSI controller.
  Msi_map_property _msi_map;
  // Mask to apply to PCI requester ID before mapping via _msi_map.
  // (maximum and default value is 0xffff)
  Int_property _msi_map_mask = 0xffffU;

  // PCI root bridge core memory. (Currently) not used.
  L4drivers::Register_block<32> _regs;

  // PCI root bridge config space.
  L4drivers::Register_block<32> _cfg;
};

class Irq_router_rs : public Resource_space
{
public:
  char const *res_type_name() const override
  { return "ECAM PCIe IRQ router"; }

  bool request(Resource *parent, ::Device *pdev,
               Resource *child, ::Device *cdev) override
  {
    auto *cd = dynamic_cast<Hw::Device *>(cdev);
    if (!cd)
      return false;

    unsigned pin = child->start();
    if (pin > 3)
      return false;

    auto *pd = dynamic_cast<Ecam_pcie_bridge *>(pdev);
    if (!pd)
      return false;

    unsigned slot = (cd->adr() >> 16);
    unsigned irq_nr = pd->int_map((pin + slot) & 3);

    d_printf(DBG_DEBUG, "%s/%08x: Requesting IRQ%c at slot %d => IRQ %d\n",
             cd->get_full_path().c_str(), cd->adr(),
             (int)('A' + pin), slot, irq_nr);

    child->del_flags(Resource::F_relative);
    child->start(irq_nr);
    child->del_flags(Resource::Irq_type_mask);
    child->add_flags(Resource::Irq_type_base | Resource::Irq_type_level_high);

    child->parent(parent);

    return true;
  }

  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to Irq_router_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Irq_router_rs\n");
    return false;
  }
};

int
Ecam_pcie_bridge::host_init()
{
  // assert on mandatory properties
  if (   assert_property(&_regs_base, "regs_base", ~0)
      || assert_property(&_regs_size, "regs_size", ~0)
      || assert_property(&_cfg_base, "cfg_base", ~0)
      || assert_property(&_cfg_size, "cfg_size", ~0)
      || assert_property(&_mmio_base, "mmio_base", ~0)
      || assert_property(&_mmio_size, "mmio_size", ~0))
    return -L4_EINVAL;

  l4_addr_t va = res_map_iomem(_regs_base, _regs_size);
  if (!va)
    {
      d_printf(DBG_ERR, "error: %s: could not map core memory.\n", name());
      return -L4_ENOMEM;
    }
  _regs = new L4drivers::Mmio_register_block<32>(va);

  va = res_map_iomem(_cfg_base, _cfg_size);
  if (!va)
    {
      d_printf(DBG_ERR, "error: %s: could not map cfg memory.\n", name());
      return -L4_ENOMEM;
    }
  _cfg = new L4drivers::Mmio_register_block<32>(va);

  // Port I/O is not performed using dedicated instructions like in/out on
  // non-x86 hosts. Instead, port I/O is emulated using an MMIO region. The
  // resources as reflected by the above device tree on ARM64 contain such a
  // region of 0x3eff0000..0x3f000000. Given that the I/O ports of separate
  // devices are located on dedicated pages (which is currently not the case,
  // see Resource_provider::min_align()) it would be possible to the map the
  // required part of the port I/O memory to the client. But it was decided
  // that on such architectures the actual memory access will be performed in
  // the IO server and port I/O performed by the client forwarded to the IO
  // server using IPC.
  // This is currently not implemented, therefore access to port I/O resources
  // of PCI devices will not work on non-x86 architectures.
  if (_ioport_base != ~0 && (_ioport_base & 0xffffffff0000ULL))
    d_printf(DBG_WARN, "%s: Base for I/O ports set to MMIO range (%08llx-%08llx)!\n",
             name(), _ioport_base.val(), _ioport_base.val() + _ioport_size.val());

  return 0;
}

int
Ecam_pcie_bridge::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width width)
{
  switch (width)
    {
    case Hw::Pci::Cfg_long:
      *value = _cfg.r<32>(addr.addr());
      break;
    case Hw::Pci::Cfg_short:
      *value = _cfg.r<16>(addr.addr());
      break;
    case Hw::Pci::Cfg_byte:
      *value = _cfg.r<8>(addr.addr());
      break;
    default:
      d_printf(DBG_WARN, "Invalid width %d!\n", width);
      return -EIO;
    }

  d_printf(DBG_ALL,
           "%s: cfg_read  addr=%02x:%02x.%x reg=%03x width=%2d-bit  =>   %0*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(), 8 << width,
           2 << width, *value & Hw::Pci::cfg_o_to_mask(width));

  return 0;
}

int
Ecam_pcie_bridge::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width width)
{
  d_printf(DBG_ALL,
           "%s: cfg_write addr=%02x:%02x.%x reg=%03x width=%2d-bit value=%0*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(), 8 << width,
           2 << width, value & Hw::Pci::cfg_o_to_mask(width));

  switch (width)
    {
    case Hw::Pci::Cfg_long:
      _cfg.r<32>(addr.addr()) = value;
      break;
    case Hw::Pci::Cfg_short:
      _cfg.r<16>(addr.addr()) = value;
      break;
    case Hw::Pci::Cfg_byte:
      _cfg.r<8>(addr.addr()) = value;
      break;
    default:
      d_printf(DBG_WARN, "Invalid width %d!\n", width);
      return -EIO;
    }

  return 0;
}

void
Ecam_pcie_bridge::init()
{
  if (host_init())
    return;

  Resource *mr;

  // I/O ports are not supported on ARM anyway.
  if (_ioport_base != ~0 && _ioport_size != ~0)
    {
      mr = new Resource_provider(Resource::Io_res);
      mr->start_size(_ioport_base & 0xffff, _ioport_size);
      mr->set_id("IO");
      add_resource_rq(mr);
    }

  mr = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw);
  mr->start_size(_mmio_base, _mmio_size);
  mr->set_id("MMIO");
  add_resource_rq(mr);

  if (_mmio_base_64 != ~0 && _mmio_size_64 != ~0)
    {
      mr = new Resource_provider(Resource::Mmio_res | Resource::Mem_type_rw
                                 | Resource::F_width_64bit);
      mr->start_size(_mmio_base_64, _mmio_size_64);
      mr->set_id("MMIO");
      add_resource_rq(mr);
    }

  auto *ir = new Hw::Pci::Irq_router_res<Irq_router_rs>();
  ir->set_id("IRQR");
  add_resource_rq(ir);

  discover_bus(this, this);

  Hw::Device::init();
}

static Hw::Device_factory_t<Ecam_pcie_bridge> f("Ecam_pcie_bridge");

}
