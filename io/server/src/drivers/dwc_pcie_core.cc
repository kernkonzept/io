/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2017, 2022-2023 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *            Frank Mehnert <frank.mehnert@kernkonzept.com>
 */
#include <l4/util/util.h>

#include "dwc_pcie_core.h"
#include "utils.h"
#include "resource_provider.h"

#include <errno.h>
#include <l4/drivers/hw_mmio_register_block>
#include <l4/re/error_helper>
#include <inttypes.h>

int
Dwc_pcie::host_init()
{
  if (   assert_property(&_cfg_base, "cfg_base", ~0)
      || assert_property(&_cfg_size, "cfg_size", ~0)
      || assert_property(&_regs_base, "regs_base", ~0)
      || assert_property(&_regs_size, "regs_size", ~0)
      || assert_property(&_mem_base, "mem_base", ~0)
      || assert_property(&_mem_size, "mem_size", ~0))
    return -L4_EINVAL;

  if (_num_lanes > 16)
    {
      d_printf(DBG_ERR, "error: %s: invalid number of PCIe lanes: %lld\n",
               name(), _num_lanes.val());
      return -L4_EINVAL;
    }

  l4_addr_t va = res_map_iomem(_regs_base, _regs_size);
  if (!va)
    {
      d_printf(DBG_ERR, "error: %s: could not map IP core memory.\n", name());
      return -L4_ENOMEM;
    }
  _regs = new L4drivers::Mmio_register_block<32>(va);

  va = res_map_iomem(_cfg_base, _cfg_size);
  if (!va)
    {
      d_printf(DBG_ERR, "error: %s: could not map config space memory.\n",
               name());
      return -L4_ENOMEM;
    }
  _cfg = new L4drivers::Mmio_register_block<32>(va);

  return L4_EOK;
}

void
Dwc_pcie::set_iatu_region(unsigned index, l4_uint64_t base_addr,
                          l4_uint32_t size, l4_uint64_t target_addr,
                          unsigned tlp_type, unsigned dir)
{
  // the default configuration of the PCIe core only has two viewports
  if (index > 1)
    return;

  if (_cpu_fixup != ~0)
    base_addr = base_addr + _cpu_fixup - _mem_base;

  _regs[Iatu_viewport] = index | (dir & Dir_mask);
  _regs[Iatu_lower_base] = base_addr & 0xffffffff;
  _regs[Iatu_upper_base] = base_addr >> 32;
  _regs[Iatu_limit] = (base_addr + size - 1) & 0xffffffff; // i.MX8: bits 12..31 ignored
  _regs[Iatu_lower_target] = target_addr & 0xffffffff;
  _regs[Iatu_upper_target] = target_addr >> 32;
  _regs[Iatu_ctrl_1] = tlp_type & Type_mask;
  _regs[Iatu_ctrl_2] = Region_enable;

  for (unsigned i = 0; i < 10; ++i)
    {
      if ((_regs[Iatu_ctrl_2] & Region_enable) == Region_enable)
        return;

      l4_usleep(10000);
    }

  d_printf(DBG_ERR, "error: %s: ATU not enabled @index %u\n", name(), index);

#ifdef ARCH_MIPS
  asm volatile ("sync" : : : "memory");
#endif
}

void
Dwc_pcie::setup_rc()
{
  // enable writing to read-only registers
  _regs[Misc_control_1].set(1U << 0);

  // set number of lanes
  unsigned lme;
  switch (_num_lanes)
    {
    case 2:  lme = Link_2_lanes;  break;
    case 4:  lme = Link_4_lanes;  break;
    case 8:  lme = Link_8_lanes;  break;
    case 16: lme = Link_16_lanes; break;
    default: lme = Link_1_lanes;  break;
    }

  _regs[Port_logic::Link_ctrl].modify(Mode_enable_mask,
                                      lme << Mode_enable_shift);

  _regs[Gen2].modify(Lane_enable_mask, _num_lanes << Lane_enable_shift);

  // disable MSI for now
  _regs[Msi_ctrl_lower_addr] = 0;
  _regs[Msi_ctrl_upper_addr] = 0;

  // setup BARs
  _regs[Hw::Pci::Config::Bar_0] = 0x00000004;
  _regs[Hw::Pci::Config::Bar_0+4] = 0x00000000;

  // setup interrupt pins
  _regs[Hw::Pci::Config::Irq_line].modify(0x0000ff00, 0x100);

  // setup bus numbers (primary = 0, secondary = 1, subordinate = 1)
  _regs[Hw::Pci::Config::Primary].modify(0x00ffffff, 0x010100);

  // setup command register: Io, Memory, Master, Serr
  _regs[Hw::Pci::Config::Command].modify(0x0000ffff, 0x107);

  // TODO: Should we get the mem bus addr from a property?
  l4_addr_t bus_addr = _mem_base;
  set_iatu_region(Iatu_vp::Idx0, _mem_base, _mem_size, bus_addr, Tlp_type::Mem);

  Resource *re = new Resource_provider(Resource::Mmio_res);
  re->alignment(0xfffff);
  re->start_end(_mem_base, _mem_base + _mem_size - 1);
  re->set_id("MMIO");
  add_resource_rq(re);

  if (_regs[Iatu_ctrl_2] != Region_enable)
    d_printf(DBG_INFO, "info: %s: iATU not enabled\n", name());

  _regs[Hw::Pci::Config::Bar_0] = 0x00000000;

  // set correct PCI class for RC
  _regs[Hw::Pci::Config::Class_rev].modify(0xffff0000, 0x0604 << 16);

  // enable directed speed change to automatically transition to Gen2 or Gen3
  // speeds after link training
  _regs[Gen2].set(1 << Speed_change_shift);

  // disable writing to read-only registers
  _regs[Misc_control_1].clear(1U << 0);
}

inline L4drivers::Register_block<32>
Dwc_pcie::cfg_regs(Cfg_addr addr)
{
  if (addr.bus() == num)
    return _regs;

  uint32_t target = ((addr.bus() << 8) | addr.devfn()) << 16;

  // TODO: add cfg type 1 access
  set_iatu_region(Iatu_vp::Idx1, _cfg_base, _cfg_size, target, Tlp_type::Cfg0);

  return _cfg;
}

int
Dwc_pcie::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width w)
{
  uint32_t v;

  if (!device_valid(addr))
    v = 0xffffffff;
  else
    v = cfg_regs(addr)[addr.reg() & ~3];

  switch (w)
    {
    case Hw::Pci::Cfg_long:
      *value = v;
      break;
    case Hw::Pci::Cfg_short:
      *value = (v >> ((addr.reg() & 3) << 3)) & 0xffff;
      break;
    case Hw::Pci::Cfg_byte:
      *value = (v >> ((addr.reg() & 3) << 3)) & 0xff;
      break;
    default:
      d_printf(DBG_WARN, "%s: Invalid width %d in cfg_read!\n", name(), w);
      return -EIO;
    }

  d_printf(DBG_ALL,
           "%s: cfg_read  addr=%02x:%02x.%x reg=%03x width=%2d-bit  =>   %0*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(), 8 << w,
           2 << w, *value & cfg_o_to_mask(w));

  return 0;
}

int
Dwc_pcie::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width w)
{
  if (!device_valid(addr))
    return 0;

  auto r = cfg_regs(addr);
  uint32_t mask, shift;

  d_printf(DBG_ALL,
           "%s: cfg_write addr=%02x:%02x.%x reg=%03x width=%2d-bit value=%0*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(), 8 << w,
           2 << w, value & cfg_o_to_mask(w));

  switch (w)
    {
    case Hw::Pci::Cfg_long:
      r[addr.reg() & ~3] = value;
      return 0;
    case Hw::Pci::Cfg_short:
      shift = (addr.reg() & 3) << 3;
      mask = 0xffff << shift;
      break;
    case Hw::Pci::Cfg_byte:
      shift = (addr.reg() & 3) << 3;
      mask = 0xff << shift;
      break;
    default:
      d_printf(DBG_WARN, "%s: Invalid width %d in cfg_write!\n", name(), w);
      return -EIO;
    }

  r[addr.reg() & ~3].modify(mask, (value << shift) & mask);
  return 0;
}

bool
Dwc_pcie::device_valid(Cfg_addr addr)
{
   // It is a specific property of the Designware PCIe core that on bus 0 the
   // PCI-to-PCI bridge is connected. To avoid stalls for any access to bus > 0
   // the link has to be up.
  if (addr.bus() != 0)
    if (!link_up())
      return false;

  // there is only the PCI-to-PCI bridge on 'our' bus and there are no other
  // devices
  if ((addr.bus() == num) && (addr.dev() > 0))
    return false;

  if ((addr.bus() > num) && (addr.dev() > 0))
    return false;

  return true;
}
