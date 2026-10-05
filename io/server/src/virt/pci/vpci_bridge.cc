/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "vpci_bridge.h"
#include "vpci_virtual_dev.h"

Vi::Pci_bridge::Dev::Dev()
{
  memset(_fns, 0, sizeof(_fns));
}

void
Vi::Pci_bridge::Dev::add_fn(Vi::Pci_dev *f)
{
  for (unsigned i = 0; i < sizeof(_fns)/sizeof(_fns[0]); ++i)
    {
      if (!_fns[i])
        {
          _fns[i] = f;
          return;
        }
    }
}

void
Vi::Pci_bridge::Dev::sort_fns()
{
  // disabled sorting because the relation of two functions is questionable
#if 0
  unsigned n;
  for (n = 0; n < sizeof(_fns)/sizeof(_fns[0]) && _fns[n]; ++n)
    ;

  if (n < 2)
    return;

  bool exchg = false;

  do
    {
      exchg = false;
      for (unsigned i = 0; i < n - 1; ++i)
        {
          if (_fns[i]->dev()->function_nr() > _fns[i+1]->dev()->function_nr())
            {
              Pci_dev *t = _fns[i];
              _fns[i] = _fns[i+1];
              _fns[i+1] = t;
              exchg = true;
            }
        }
      n -= 1;
    }
  while (exchg && n >= 1);
#endif
}

void
Vi::Pci_bridge::Bus::add_fn(Vi::Pci_dev *pd, int slot)
{
  if (slot >= 0)
    {
      _devs[slot].add_fn(pd);
      _devs[slot].sort_fns();
      return;
    }

  for (unsigned d = 0; d < Devs && !_devs[d].empty(); ++d)
    if (_devs[d].cmp(pd))
      {
        _devs[d].add_fn(pd);
        _devs[d].sort_fns();
        return;
      }

  for (unsigned d = 0; d < Devs; ++d)
    if (_devs[d].empty())
      {
        _devs[d].add_fn(pd);
        return;
      }
}

void
Vi::Pci_bridge::add_child(Vi::Device *d)
{
  Vi::Pci_dev *vp = d->find_feature<Vi::Pci_dev>();

  // hm, we do currently not add non PCI devices.
  if (!vp)
    return;

  _bus.add_fn(vp);
  Vi::Device::add_child(d);
}


void
Vi::Pci_bridge::add_child_fixed(Vi::Device *d, Vi::Pci_dev *vp, unsigned dn,
                                unsigned fn)
{
  _bus.dev(dn)->fn(fn, vp);
  Vi::Device::add_child(d);
}


Vi::Pci_bridge *
Vi::Pci_bridge::find_bridge(unsigned bus)
{
  if (0)
    printf("PCI[%p]: look for bridge for bus %x %02x %02x\n",
           this, bus, _subordinate, _secondary);
  if (bus == _secondary)
    return this;

  if (bus < _secondary || bus > _subordinate)
    return 0;

  for (unsigned d = 0; d < Bus::Devs; ++d)
    for (unsigned f = 0; f < Dev::Fns; ++f)
      {
        Vi::Pci_dev *p = _bus.dev(d)->fn(f);
        if (!p)
          break;

        Vi::Pci_bridge *b = dynamic_cast<Vi::Pci_bridge*>(p);
        if (b && (b = b->find_bridge(bus)))
          return b;
      }

  return 0;
}


Vi::Pci_dev *
Vi::Pci_bridge::child_dev(unsigned bus, unsigned char dev, unsigned char fn)
{
  Vi::Pci_bridge *b = find_bridge(bus);
  if (!b)
    return 0;

  if (dev >= Bus::Devs || fn >= Dev::Fns)
    return 0;

  return b->_bus.dev(dev)->fn(fn);
}
