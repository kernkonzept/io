/*
 * Copyright (C) 2017, 2022-2025 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *            Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include <l4/re/error_helper>
#include <l4/util/util.h>

#include "dwc_pcie_core.h"
#include "utils.h"
#include "resource_provider.h"

#include <errno.h>
#include <inttypes.h>

bool
Dwc_pcie::host_init()
{
  if (   assert_property(&_cfg_base, "cfg_base", ~0)
      || assert_property(&_cfg_size, "cfg_size", ~0)
      || assert_property(&_regs_base, "regs_base", ~0)
      || assert_property(&_regs_size, "regs_size", ~0)
      || assert_property(&_mem_base, "mem_base", ~0)
      || assert_property(&_mem_size, "mem_size", ~0))
    return false;

  if (_num_lanes > 16)
    {
      error("invalid number of PCIe lanes: %lld.", _num_lanes.val());
      return false;
    }

  if (l4_addr_t va = res_map_iomem(_regs_base, _regs_size))
    _regs = new L4drivers::Mmio_register_block<32>(va);
  else
    {
      error("could not map IP core memory.");
      return false;
    }

  if (l4_addr_t va = res_map_iomem(_cfg_base, _cfg_size))
    _cfg = new L4drivers::Mmio_register_block<32>(va);
  else
    {
      error("could not map config space memory.");
      return false;
    }

  if (!controller_host_init())
    return false;

  _offs_cap_pcie = get_pci_cap_offs(Hw::Pci::Cap::Pcie);

  // CSR=0 on DWC PCIe older than 4.70a
  _pci_version = _regs[Port_logic::Pcie_version_number].read();
  l4_uint32_t version_type = _regs[Port_logic::Pcie_version_type].read();

  if (_pci_version >= 0x480a)
    _iatu_unroll_enabled = true;
  else if (_pci_version == 0 && _regs[Port_logic::Iatu_viewport] == 0xffff'ffff)
    _iatu_unroll_enabled = true;

  if (_iatu_unroll_enabled)
    {
      if (   assert_property(&_atu_base, "atu_base", ~0)
          || assert_property(&_atu_size, "atu_size", ~0))
        {
          error("iATU unroll enabled but no atu_base/size provided.");
          return false;
        }

      if (l4_addr_t va = res_map_iomem(_atu_base, _atu_size))
        _atu = new L4drivers::Mmio_register_block(va);
      else
        {
          error("could not map 'atu' memory.");
          return false;
        }
    }

  if (_pci_version)
    d_printf(DBG_INFO, "DWC PCIe version '%c%c%c%c-%c%c%c%c', unroll %sabled.\n",
             (_pci_version >> 24) & 0xff, (_pci_version >> 16) & 0xff,
             (_pci_version >>  8) & 0xff, (_pci_version >>  0) & 0xff,
             (version_type >> 24) & 0xff, (version_type >> 16) & 0xff,
             (version_type >>  8) & 0xff, (version_type >>  8) & 0xff,
             _iatu_unroll_enabled ? "en" : "dis");
  else
    d_printf(DBG_INFO, "DWC PCIe unknown version, unroll %sabled.\n",
             _iatu_unroll_enabled ? "en" : "dis");

  unsigned max_region = cxx::min<unsigned>(_atu_size / 512, 256);
  unsigned i;
  for (i = 0; i < max_region; ++i)
    {
      unsigned offs = Atu::Unr_lower_target + (i << 9);
      _atu[offs] = 0x1111'0000;
      if (_atu[offs] != 0x1111'0000)
        break;
    }
  _num_ob_windows = i;

  for (i = 0; i < max_region; ++i)
    {
      unsigned offs = Atu::Unr_lower_target + (i << 9) + (1 << 8);
      _atu[offs] = 0x1111'0000;
      if (_atu[offs] != 0x1111'0000)
        break;
    }
  _num_ib_windows = i;

  return true;
}

void
Dwc_pcie::set_iatu_region(unsigned index, l4_uint64_t base_addr,
                          l4_uint64_t size, l4_uint64_t target_addr,
                          unsigned tlp_type, unsigned dir)
{
  if (_cpu_fixup != ~0)
    base_addr += _cpu_fixup - _mem_base;

  l4_uint64_t limit_addr = base_addr + size - 1;
  l4_uint32_t func_no = 0; // RC mode: 0, EP mode: != 0.

  // Since DWC PCIe Core version 4.80, iATU supports an "iATU Unroll Mode".
  // "Legacy Mode" is still supported if configured to do so, but the standard
  // is "Unroll Mode".
  // Legacy mode apparently select by setting Iatu_viewport != 0xffff'ffff.
  if (_iatu_unroll_enabled)
    {
      unsigned offs = (index << 9) + (!!dir << 8);

      _atu[offs + Atu::Unr_lower_base] = base_addr & 0xffff'ffff;
      _atu[offs + Atu::Unr_upper_base] = base_addr >> 32;
      _atu[offs + Atu::Unr_lower_limit] = limit_addr & 0xffff'ffff;
      _atu[offs + Atu::Unr_upper_limit] = limit_addr >> 32;
      _atu[offs + Atu::Unr_lower_target] = target_addr & 0xffff'ffff;
      _atu[offs + Atu::Unr_upper_target] = target_addr >> 32;
      l4_uint32_t ctrl1 = (tlp_type & Type_mask) | (func_no << 20);
      if (limit_addr >> 32 > base_addr >> 32)
        ctrl1 |= (1 << 13); // 0: max limit 4 GiB
      if (_pci_version == 0x490a || _pci_version == 0x3530302A)
        ctrl1 |= (1 << 8); // PCIE_ATU_TD
      _atu[offs + Atu::Unr_ctrl_1] = ctrl1;
      _atu[offs + Atu::Unr_ctrl_2] = Region_enable;

      if (index != 1) // avoid extensive logging during cfg_reads()
        d_printf(DBG_DEBUG,
                 "Dwc_pcie::set_iatu_region: %08llx-%08llx => %08llx-%08llx\n",
                 base_addr, limit_addr, target_addr, target_addr + size - 1);

      for (unsigned i = 0; i < 10; ++i)
        {
          if ((_atu[offs + Atu::Unr_ctrl_2] & Region_enable) == Region_enable)
            return;

          l4_usleep(10'000);
        }
    }
  else
    {
      // The default configuration of the PCIe core only has two viewports.
      // XXX Tegra234 has 8 outbound regions!
      if (index > 1)
        return;

      _regs[Port_logic::Iatu_viewport] = index | (dir & Dir_mask);
      _regs[Port_logic::Iatu_lower_base] = base_addr & 0xffff'ffff;
      _regs[Port_logic::Iatu_upper_base] = base_addr >> 32;
      // i.MX8: bits 12..31 ignored!
      _regs[Port_logic::Iatu_limit] = limit_addr & 0xffff'ffff;
      _regs[Port_logic::Iatu_lower_target] = target_addr & 0xffff'ffff;
      // only for PCIe version >= 0x460a!
      _regs[Port_logic::Iatu_upper_target] = target_addr >> 32;
      l4_uint32_t ctrl1 = (tlp_type & Type_mask) | (func_no << 20);
      // if (_pci_version >= 0x460a && upper_32(limit) > upper_32(cpu_addr))
      //   ctrl1 |= PCIE_ATU_INCREASE_REGION_SIZE;
      // if (_pci_version == 0x490a || _pci_version == 0x3530302A)
      //   ctrl1 |= dw_pcie_enable_ecrc();
      _regs[Port_logic::Iatu_ctrl_1] = ctrl1;
      _regs[Port_logic::Iatu_ctrl_2] = Region_enable;

      for (unsigned i = 0; i < 10; ++i)
        {
          if ((_regs[Port_logic::Iatu_ctrl_2] & Region_enable) == Region_enable)
            return;

          l4_usleep(10'000);
        }
    }

  error("ATU not enabled @index %u", index);

#ifdef ARCH_MIPS
  asm volatile ("sync" : : : "memory");
#endif
}

bool
Dwc_pcie::setup_rc()
{
  // enable writing to read-only registers
  _regs[Port_logic::Misc_control_1].set(1U << 0);

  if (_max_link_speed > 4)
    {
      error("property 'max_link_speed' out of range [0..4].");
      return false;
    }

  if (_max_link_speed > 0)
    {
      // PCIe capability: Link Capabilities Register
      l4_uint32_t cap = _regs[_offs_cap_pcie + 0xc];
      // PCIe capability: Link Control 2 Register
      l4_uint32_t ctrl2 = _regs[_offs_cap_pcie + 0x30] & ~0xf;
      l4_uint32_t lnk_speed;
      switch (_max_link_speed)
        {
        case 1: lnk_speed = 1; break; // 2.5GT/s
        case 2: lnk_speed = 2; break; // 5GT/s
        case 3: lnk_speed = 3; break; // 8GT/s
        case 4: lnk_speed = 4; break; // 16GT/s
        case 5: lnk_speed = 5; break; // 32GT/s
        case 6: lnk_speed = 6; break; // 64GT/s
        default:
          lnk_speed = (cap & 0xf);
          ctrl2 &= 0x20;
          break; // hardware capability
        }
      // PCIe capability: Link Capabilities Register
      _regs[_offs_cap_pcie + 0xc].modify(0xf, lnk_speed);
      // PCIe capability: Link Control 2 Register
      _regs[_offs_cap_pcie + 0x30] = ctrl2 | lnk_speed;
    }

  // Configure Gen1 number of Fast Training Sequence ordered sets (N_FTS)
  if (_nft_gen1)
    _regs[Port_logic::Afr].modify(0xff'ff00, _nft_gen1 << 8 | _nft_gen1 << 16);

  // Configure Gen2+ number of Fast Training Sequence ordered sets (N_FTS)
  if (_nft_gen2)
    _regs[Port_logic::Gen2].modify(0xff << 0, _nft_gen2 << 0);

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

  _regs[Port_logic::Link_ctrl]
    .modify(Pl_link_ctrl::Mode_enable_mask, lme << Pl_link_ctrl::Mode_enable_shift);

  _regs[Port_logic::Gen2]
    .modify(Pl_gen2::Lane_enable_mask, _num_lanes << Pl_gen2::Lane_enable_shift);

  // disable MSI for now
  _regs[Port_logic::Msi_ctrl_lower_addr] = 0;
  _regs[Port_logic::Msi_ctrl_upper_addr] = 0;

  // setup RC BARs
  _regs[Hw::Pci::Config::Bar_0] = 0x00000004;
  _regs[Hw::Pci::Config::Bar_0+4] = 0x00000000;

  // setup interrupt pins to INTA#.
  _regs[Hw::Pci::Config::Irq_line].modify(0x0000ff00, 0x100);

  // setup bus numbers (primary = 0, secondary = 1, subordinate = 1)
  _regs[Hw::Pci::Config::Primary].modify(0x00ffffff, 0x010100);

  // setup command register: Io, Memory, Master, Serr
  _regs[Hw::Pci::Config::Command].modify(0x0000ffff, 0x107);

  // Disable all outbound windows.
  for (unsigned i = 0; i < _num_ob_windows; ++i)
    {
      if (_iatu_unroll_enabled)
        _atu[Atu::Unr_ctrl_2 + (i << 9)] = ~(1U << 31);
      else
        {
          _regs[Port_logic::Iatu_viewport] = Iatu_vp::Outbound | i;
          _regs[Port_logic::Iatu_ctrl_2] = ~(1U << 31);
        }
    }

  if (_bus_base != ~0 && _bus_base != _mem_base)
    {
      error("PCI bridge windows with different bus address not supported.");
      return false;
    }

  l4_addr_t bus_addr = _mem_base;
  set_iatu_region(Iatu_vp::Idx0, _mem_base, _mem_size, bus_addr, Tlp_type::Mem);

  Resource *re = new Resource_provider(Resource::Mmio_res);
  re->alignment(0xfffff);
  re->start_end(_mem_base, _mem_base + _mem_size - 1);
  re->set_id("MMIO");
  add_resource_rq(re);

  if (!_iatu_unroll_enabled)
    {
      if (_regs[Port_logic::Iatu_ctrl_2] != Region_enable)
        d_printf(DBG_INFO, "info: %s: iATU not enabled\n", name());
    }

  _regs[Hw::Pci::Config::Bar_0] = 0x00000000;

  // set correct PCI class for RC
  _regs[Hw::Pci::Config::Class_rev].modify(0xffff << 16, 0x0604 << 16);

  // enable directed speed change to automatically transition to Gen2 or Gen3
  // speeds after link training
  _regs[Port_logic::Gen2].set(1 << Speed_change_shift);

  // disable writing to read-only registers
  _regs[Port_logic::Misc_control_1].clear(1U << 0);

  return true;
}

inline L4drivers::Register_block<32>
Dwc_pcie::cfg_regs(Cfg_addr addr)
{
  if (addr.bus() == secondary)
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
           "%s: cfg_read  addr=%02x:%02x.%x reg=%04x => %*.*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(),
           2 << w, 2 << w, *value & cfg_o_to_mask(w));

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
           "%s: cfg_write addr=%02x:%02x.%x reg=%04x value=%*.*x\n",
           name(), addr.bus(), addr.dev(), addr.fn(), addr.reg(),
           2 << w, 2 << w, value & cfg_o_to_mask(w));

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
  if ((addr.bus() == secondary) && (addr.dev() > 0))
    return false;

  if ((addr.bus() > secondary) && (addr.dev() > 0))
    return false;

  return true;
}

l4_uint8_t
Dwc_pcie::get_pci_cap_offs(l4_uint8_t cap_id) const
{
  l4_uint8_t offs = _regs.r<8>(Hw::Pci::Config::Capability_ptr);
  while (offs)
    {
      l4_uint16_t reg = _regs.r<16>(offs);
      if ((reg & 0xff) == cap_id)
        return offs;
      offs = (reg & 0xff00) >> 8;
    }

  return 0;
}

l4_uint16_t
Dwc_pcie::get_pci_ext_cap_offs(l4_uint8_t cap_id) const
{
  l4_uint16_t offs = 256;
  for (unsigned ttl = (0x1000U - 256U) / 8; ttl-- > 0 && offs >= 256;)
    {
      l4_uint32_t hdr = _regs.r<32>(offs);
      if (hdr == 0)
        return 0;
      if ((hdr & 0xffff) == cap_id)
        return offs;
      offs = (hdr >> 20) & 0xffc;
    }

  return 0;
}
