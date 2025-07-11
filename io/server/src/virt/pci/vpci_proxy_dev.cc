/*
 * Copyright (C) 2010-2020, 2022, 2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <cassert>

#include <debug.h>
#include <pci-caps.h>
#include <pci-dev.h>

#ifdef CONFIG_L4IO_PCIID_DB
# include <pciids.h>
#endif

#include "virt/vbus_factory.h"
#include "vpci_proxy_dev.h"
#include "vpci_sriov.h"

namespace Vi {

bool
Pci_proxy_dev::scan_pci_caps()
{
  l4_uint8_t pci_cap = _hwf->config().read<l4_uint8_t>(Hw::Pci::Config::Capability_ptr);
  bool is_pci_express = false;
  while (pci_cap)
    {
      l4_uint16_t cap = _hwf->config().read<l4_uint16_t>(pci_cap);
      switch (cap & 0xff)
        {
        case Hw::Pci::Cap::Pcie:
          is_pci_express = true;
          /* fall through */
        case Hw::Pci::Cap::Pcix:
        case Hw::Pci::Cap::Msi:
        case Hw::Pci::Cap::Msi_x:
        case Hw::Pci::Cap::Vndr:
        default:
          add_pci_cap(new Pci_proxy_cap(_hwf, pci_cap));
          break;
        }

      pci_cap = cap >> 8;
    }
  return is_pci_express;
}

namespace {
    struct Pcie_dummy_cap : Pcie_capability
    {
      Pcie_dummy_cap(unsigned offset, unsigned sz)
      : Pcie_capability(offset)
      {
        set_cap(0xfe); // reserved ID
        set_size(sz);
      }

      int cap_read(l4_uint32_t, l4_uint32_t *v, Cfg_width) override
      {
        *v = 0xffffffff;
        return 0;
      }

      int cap_write(l4_uint32_t, l4_uint32_t, Cfg_width) override
      {
        return 0;
      }
    };
}

l4_uint16_t
Pci_proxy_dev::_skip_pcie_cap(Hw::Pci::Extended_cap const &cap, unsigned sz)
{
  // create "fake" capability with a reserved ID, according to
  // PCI-SIG IDs >0x2c are reserved
  add_pcie_cap(new Pcie_dummy_cap(cap.reg(), sz));
  return cap.next();
}

void
Pci_proxy_dev::scan_pcie_caps()
{
  l4_uint16_t offset = 0x100;
  while (offset)
    {
      Hw::Pci::Extended_cap cap = _hwf->config(offset);

      // the device doesn't have extended capabilities
      if (offset == 0x100 && !cap.is_valid())
        return;

      // hide special extended capabilities from virtual devices
      switch (cap.id())
        {
        default:
          break;

        case Hw::Pci::Sr_iov_cap::Id:
#ifdef CONFIG_L4IO_PCI_SRIOV
          add_pcie_cap(new Sr_iov_proxy_cap(_hwf, cap.header(), offset, offset));
          offset = cap.next();
#else
          offset = _skip_pcie_cap(cap, Hw::Pci::Sr_iov_cap::Size);
#endif
          continue;
        case Hw::Pci::Extended_cap::Acs:
          offset = _skip_pcie_cap(cap, 8);
          continue;
        case Hw::Pci::Resizable_bar_cap::Id:
          {
            auto ctrl0 = cap.read<Hw::Pci::Resizable_bar_cap::Bar_ctrl_0>();
            unsigned size = 4 + 8 * ctrl0.num_bars();
            offset = _skip_pcie_cap(cap, size);
            continue;
          }
        }

      add_pcie_cap(new Pcie_proxy_cap(_hwf, cap.header(), offset, offset));
      offset = cap.next();
    }

  // if the device has extended capabilities there must be one at offset 0x100
  assert (!_pcie_caps || _pcie_caps->offset() == 0x100);
#if 0
  if (_pcie_caps && _pcie_caps->offset() != 0x100)
    _pcie_caps->set_offset(0x100);
#endif
}

Pci_proxy_dev::Pci_proxy_dev(Hw::Pci::If *hwf)
: _hwf(hwf), _rom(0)
{
  assert (hwf);
  for (int i = 0; i < 6;)
    i += _vbars.from_resource(i, _hwf->bar(i));

  if (_hwf->rom())
    _rom = _hwf->rom()->start();

  if (scan_pci_caps())
    scan_pcie_caps();
}

int
Pci_proxy_dev::irq_enable(Irq_info *irq)
{
  for (Resource_list::const_iterator i = host()->resources()->begin();
       i != host()->resources()->end(); ++i)
    {
      Resource *res = *i;

      if (!res->disabled() && res->type() == Resource::Irq_res)
        {
          irq->irq = res->start();
          irq->trigger = !res->irq_is_level_triggered();
          irq->polarity = res->irq_is_low_polarity();
          d_printf(DBG_DEBUG, "Enable IRQ: irq=%d trg=%x pol=%x\n",
                   irq->irq, irq->trigger, irq->polarity);
          if (dlevel(DBG_DEBUG2))
            dump();
          return 0;
        }
    }
  return -L4_EINVAL;
}

void
Pci_proxy_dev::write_rom(l4_uint32_t v)
{
  Hw::Pci::If *p = _hwf;

  if (0)
    printf("write ROM bar %x %p\n", v, p->rom());
  Resource *r = p->rom();
  if (!r)
    return;

  l4_uint64_t size_mask = r->alignment();

  _rom = (_rom & size_mask) | (v & (~size_mask | 1));

  p->cfg_write(0x30, (r->start() & ~1U) | (v & 1), Hw::Pci::Cfg_long);
}

Pci_capability *
Pci_proxy_dev::find_pci_cap(unsigned offset) const
{
  if (!_pci_caps || offset < 0x3c)
    return 0;

  for (Pci_capability *c = _pci_caps; c; c = c->next())
    if (c->is_inside(offset))
      return c;

  return 0;
}

void
Pci_proxy_dev::add_pci_cap(Pci_capability *c)
{
  Pci_capability **i;
  for (i = &_pci_caps; *i; i = &(*i)->next())
    if ((*i)->offset() > c->offset())
      break;

  c->next() = *i;
  *i = c;
}

Pcie_capability *
Pci_proxy_dev::find_pcie_cap(unsigned offset) const
{
  if (!_pcie_caps || offset < 0x100)
    return 0;

  for (Pcie_capability *c = _pcie_caps; c; c = c->next())
    if (c->is_inside(offset))
      return c;

  return 0;
}

void
Pci_proxy_dev::add_pcie_cap(Pcie_capability *c)
{
  Pcie_capability **i;
  for (i = &_pcie_caps; *i; i = &(*i)->next())
    if ((*i)->offset() > c->offset())
      break;

  c->next() = *i;
  *i = c;
}

int
Pci_proxy_dev::cfg_read(l4_uint32_t reg, l4_uint32_t *v, Cfg_width order)
{
  Hw::Pci::If *p = _hwf;

  l4_uint32_t buf;

  l4_uint32_t const *r = &buf;
  reg &= ~0U << order;
  int dw_reg = reg & ~3;

  if (Pci_capability *cap = find_pci_cap(dw_reg))
    return cap->cfg_read(reg, v, order);

  if (Pcie_capability *cap = find_pcie_cap(dw_reg))
    return cap->cfg_read(reg, v, order);

  switch (dw_reg)
    {
    case 0x00: buf = p->vendor_device_ids(); break;
    case 0x08: buf = p->class_rev(); break;
    case 0x04: buf = p->checked_cmd_read(); break;
    /* simulate multi function on hdr type */
    case 0x0c: buf = p->config().read<l4_uint32_t>(dw_reg) | 0x00800000; break;
    case 0x10: /* bars 0 to 5 */
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24: buf = _vbars.read(reg - 0x10, order); break;
    case Hw::Pci::Config::Subsys_vendor: /* offset 0x2c */
               buf = p->subsys_vendor_ids();
               break;
    case Hw::Pci::Config::Rom_address: /* offset 0x30 */
               buf = read_rom();
               break;
    case Hw::Pci::Config::Capability_ptr: /* offset 0x34 */
               if (_pci_caps)
                 buf = _pci_caps->offset();
               else
                 buf = 0;
               break;
    case 0x38: buf = 0; break; /* PCI 3.0 reserved */

    case 0x100: /* empty PCIe cap */
               buf = 0xffff; break;
    default:   if (0) printf("pass unknown PCI cfg read for device %s: %x\n",
                             host()->name(), reg);
               /* fall through */
    case 0x28:
    case 0x3c:
               /* pass through the rest ... */
               buf = p->config().read<l4_uint32_t>(dw_reg);
               break;
    }

  unsigned mask = ~0U >> (32 - (8U << order));
  *v = (*r >> ((reg & 3) *8)) & mask;
  return L4_EOK;
}


void
Pci_proxy_dev::_do_cmd_write(unsigned mask,unsigned value)
{
  _hwf->checked_cmd_write(mask, value);
}

int
Pci_proxy_dev::_do_status_cmd_write(l4_uint32_t mask, l4_uint32_t value)
{
  if (mask & 0xffff)
    _do_cmd_write(mask & 0xffff, value & 0xffff);

  // status register has 'write 1 to clear' semantics so we can always write
  // 16bit with the bits masked that we do not want to affect.
  if (mask & value & 0xffff0000)
    _hwf->cfg_write(Pci::Config::Status, (value & mask) >> 16, Hw::Pci::Cfg_short);

  return 0;
}

int
Pci_proxy_dev::_do_rom_bar_write(l4_uint32_t mask, l4_uint32_t value)
{
  l4_uint32_t b = read_rom();

  if ((value & mask & 1) && !(b & mask & 1) && !_hwf->enable_rom())
    return 0;  // failed to enable

  b &= ~mask;
  b |= value & mask;
  write_rom(b);
  return 0;
}


int
Pci_proxy_dev::cfg_write(l4_uint32_t reg, l4_uint32_t v, Cfg_width order)
{
  Hw::Pci::If *p = _hwf;

  reg &= ~0U << order;
  l4_uint32_t const offset_32 = reg & 3;
  l4_uint32_t const mask_32 = (~0U >> (32 - (8U << order))) << (offset_32 * 8);
  l4_uint32_t const value_32 = v << (offset_32 * 8);

  if (Pci_capability *cap = find_pci_cap(reg & ~3))
    return cap->cfg_write(reg, v, order);

  if (Pcie_capability *cap = find_pcie_cap(reg & ~3))
    return cap->cfg_write(reg, v, order);

  switch (reg & ~3)
    {
    case 0x00: return 0;
    case 0x08: return 0;
    case 0x04: return _do_status_cmd_write(mask_32, value_32);
    case 0x10: /* bars 0 to 5 */
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24: _vbars.write(reg - 0x10, v, order); return 0;
    case Hw::Pci::Config::Subsys_vendor:  return 0;
    case Hw::Pci::Config::Rom_address:    return _do_rom_bar_write(mask_32, value_32);
    case Hw::Pci::Config::Capability_ptr: return 0;
    case 0x38: return 0;
    /* pass through the rest ... */
    default:   if (0) printf("pass unknown PCI cfg write for device %s: %x\n",
                             host()->name(), reg);
               /* fall through */
    case 0x0c:
    case 0x28:
    case 0x3c: return p->cfg_write(reg, v, order);
    }
}

void
Pci_proxy_dev::dump() const
{
  Hw::Pci::If *p = _hwf;

  printf("       %04x:%02x:%02x.%x:\n",
         0, p->bus_nr(), _hwf->device_nr(), _hwf->function_nr());
#if 0
#ifdef CONFIG_L4IO_PCIID_DB
  char buf[130];
  libpciids_name_device(buf, sizeof(buf), _dev->vendor(), _dev->device());
  printf("              %s\n", buf);
#endif
#endif
}

Pci_dev::Msi_src *
Pci_proxy_dev::msi_src() const
{
  assert (hwf());
  return hwf()->get_msi_src();
}

namespace {
  static Feature_factory_t<Pci_proxy_dev, Hw::Pci::Dev> __pci_f_factory1;
}

}
