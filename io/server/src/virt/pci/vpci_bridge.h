/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "vpci.h"

namespace Vi {

/**
 * A basic virtual PCI bridge.
 *
 * This class is the base for virtual Host-to-PCI bridges, for virtual
 * PCI-to-PCI bridges, and also for this such as virtual PCI-to-Cardbus bridges.
 */
class Pci_bridge : public Device
{
public:
  class Dev
  {
  public:
    enum { Fns = 8 };

  private:
    Pci_dev *_fns[Fns];

  public:
    Dev();

    bool empty() const { return !_fns[0]; }
    void add_fn(Pci_dev *f);
    void sort_fns();

    Pci_dev *fn(unsigned f) const { return _fns[f]; }
    void fn(unsigned f, Pci_dev *fn) { _fns[f] = fn; }
    bool cmp(Pci_dev const *od) const
    {
      if (empty())
        return false;

      return _fns[0]->is_same_device(od);
    }
  };

  class Bus
  {
  public:
    enum { Devs = 32 };

  private:
    Dev _devs[Devs];

  public:
    Dev const *dev(unsigned slot) const { return &_devs[slot]; }
    Dev *dev(unsigned slot) { return &_devs[slot]; }

    void add_fn(Pci_dev *d, int slot = -1);
  };

  Pci_bridge() : _free_dev(0), _primary(0), _secondary(0), _subordinate(0) {}

protected:
  Bus _bus;
  unsigned _free_dev;
  union
  {
    struct
    {
      l4_uint32_t _primary:8;
      l4_uint32_t _secondary:8;
      l4_uint32_t _subordinate:8;
      l4_uint32_t _lat:8;
    };
    l4_uint32_t _bus_config;
  };

public:

  void primary(unsigned char v) { _primary = v; }
  void secondary(unsigned char v) { _secondary = v; }
  void subordinate(unsigned char v) { _subordinate = v; }
  Pci_dev *child_dev(unsigned bus, unsigned char dev, unsigned char fn);
  void add_child(Device *d) override;
  void add_child_fixed(Device *d, Pci_dev *vp, unsigned dn, unsigned fn);

  Pci_bridge *find_bridge(unsigned bus);
  void setup_bus();
  void finalize_setup() override;

};

}
