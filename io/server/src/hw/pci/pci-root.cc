/*
 * Copyright (C) 2010-2020, 2023-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <pci-root.h>
#include <vector>

#if defined(ARCH_x86) || defined(ARCH_amd64)

#include <l4/util/port_io.h>
#include <utils.h>

#endif

#if defined(ARCH_x86) || defined(ARCH_amd64)

int
Hw::Pci::Port_root_bridge::cfg_read(Hw::Pci::Cfg_addr addr, l4_uint32_t *value,
                                    Hw::Pci::Cfg_width w)
{
  if (addr.reg() >= 0x100)
    {
      *value = ~0U;
      return 0;
    }

  Pthread_mutex_guard g(&_cfg_lock);
  l4util_out32((addr.to_compat_addr() | 0x80000000) & ~3UL, 0xcf8);

  switch (w)
    {
    case Hw::Pci::Cfg_byte:  *value = l4util_in8(0xcfc + addr.reg_offs(w)); break;
    case Hw::Pci::Cfg_short: *value = l4util_in16(0xcfc + addr.reg_offs(w)); break;
    case Hw::Pci::Cfg_long:  *value = l4util_in32(0xcfc); break;
    }
  return 0;
}

int
Hw::Pci::Port_root_bridge::cfg_write(Hw::Pci::Cfg_addr addr, l4_uint32_t value,
                                     Hw::Pci::Cfg_width w)
{
  if (addr.reg() >= 0x100)
    return 0;

  Pthread_mutex_guard g(&_cfg_lock);
  l4util_out32((addr.to_compat_addr() | 0x80000000) & ~3UL, 0xcf8);
  switch (w)
    {
    case Hw::Pci::Cfg_byte:  l4util_out8(value, 0xcfc + addr.reg_offs(w)); break;
    case Hw::Pci::Cfg_short: l4util_out16(value, 0xcfc + addr.reg_offs(w)); break;
    case Hw::Pci::Cfg_long:  l4util_out32(value, 0xcfc); break;
    }
  return 0;
}

#endif

int
Hw::Pci::Mmio_root_bridge::cfg_read(Hw::Pci::Cfg_addr addr, l4_uint32_t *value,
                                    Hw::Pci::Cfg_width w)
{
  switch (w)
    {
    case Hw::Pci::Cfg_byte:  *value = *(volatile l4_uint8_t  *)a(addr); break;
    case Hw::Pci::Cfg_short: *value = *(volatile l4_uint16_t *)a(addr); break;
    case Hw::Pci::Cfg_long:  *value = *(volatile l4_uint32_t *)a(addr); break;
    }
  return 0;
}

int
Hw::Pci::Mmio_root_bridge::cfg_write(Hw::Pci::Cfg_addr addr, l4_uint32_t value,
                                     Hw::Pci::Cfg_width w)
{
  switch (w)
    {
    case Hw::Pci::Cfg_byte:  *(volatile l4_uint8_t  *)a(addr) = value; break;
    case Hw::Pci::Cfg_short: *(volatile l4_uint16_t *)a(addr) = value; break;
    case Hw::Pci::Cfg_long:  *(volatile l4_uint32_t *)a(addr) = value; break;
    }

  return 0;
}

static std::vector<Hw::Pci::Root_bridge *> __pci_root_bridge;

void
Hw::Pci::Root_bridge::setup(Hw::Device *host)
{
  for (Resource_list::const_iterator i = host->resources()->begin();
       i != host->resources()->end(); ++i)
    if ((*i)->type() == Resource::Bus_res)
      {
        secondary = (*i)->start();
        subordinate = (*i)->end();
      }

  Hw::Pci::Bridge_base::discover_bus(host, this);
}

Hw::Pci::Root_bridge *
Hw::Pci::root_bridge(unsigned segment)
{
  for (auto b: __pci_root_bridge)
    if (b->segment() == segment)
      return b;

  return 0;
}

int
Hw::Pci::register_root_bridge(Hw::Pci::Root_bridge *b)
{
  for (auto x: __pci_root_bridge)
    if (x->segment() == b->segment()
        && x->secondary == b->secondary)
      return -L4_EEXIST;

  __pci_root_bridge.push_back(b);
  return 0;
}

Hw::Pci::Root_bridge *
Hw::Pci::find_root_bridge(unsigned segment, int bus)
{
  for (auto b: __pci_root_bridge)
    if (b->segment() == segment
        && b->secondary == bus)
      return b;

  return 0;
}
