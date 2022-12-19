/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020, 2022 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <pci-bridge.h>

#include <pthread.h>

namespace Hw { namespace Pci {

class Root_bridge : public Dev_feature, public Bridge_base, public Config_space
{
private:
  Hw::Device *_host;
  unsigned _segment;

public:
  explicit Root_bridge(unsigned segment, unsigned bus_nr, Hw::Device *host)
  : Bridge_base(bus_nr), _host(host), _segment(segment)
  {}


  void set_host(Hw::Device *host) { _host = host; }

  Hw::Device *host() const { return _host; }

  void setup(Hw::Device *host) override;

  unsigned alloc_bus_number() override
  {
    return ++subordinate;
  }

  unsigned segment() const override
  { return _segment; }
};

struct Port_root_bridge : public Root_bridge
{
  explicit Port_root_bridge(unsigned segment, unsigned bus_nr,
                            Hw::Device *host)
  : Root_bridge(segment, bus_nr, host) {}

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

private:
  pthread_mutex_t _cfg_lock = PTHREAD_MUTEX_INITIALIZER;
};

struct Mmio_root_bridge : public Root_bridge
{
  explicit Mmio_root_bridge(unsigned segment, unsigned bus_nr,
                            Hw::Device *host,
                            l4_uint64_t phys_base, unsigned num_busses)
  : Root_bridge(segment, bus_nr, host)
  {
    _mmio = res_map_iomem(phys_base, num_busses * (1 << 20));
    if (!_mmio)
      throw("ne");
  }

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

  l4_addr_t a(Cfg_addr addr) const { return _mmio + addr.addr(); }

  l4_addr_t _mmio;
};

Root_bridge *root_bridge(unsigned segment);
Root_bridge *find_root_bridge(unsigned segment, int bus);
int register_root_bridge(Root_bridge *b);

} }
