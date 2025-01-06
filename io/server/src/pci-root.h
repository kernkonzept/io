/*
 * Copyright (C) 2010-2020, 2022-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <pci-bridge.h>

#include <pthread.h>

namespace Hw { namespace Pci {

class Root_bridge : public Dev_feature, public Bridge_base, public Config_space
{
private:
  Hw::Device *_host;
  Platform_adapter_if *_platform_adapter;
  unsigned _segment;

protected:
  int translate_msi_src(If *dev, l4_uint64_t *si) override
  {
    // Forward to platform provided MSI translator
    if (_platform_adapter)
      return _platform_adapter->translate_msi_src(dev, si);
    else
      return -L4_ENODEV;
  }

  int translate_dma_src(Dma_requester_id rid, l4_uint64_t *si) const override
  {
    // Forward to platform provided DMA translator
    if (_platform_adapter)
      return _platform_adapter->translate_dma_src(rid, si);
    else
      return -L4_ENODEV;
  }

  int map_msi_src(If *dev, l4_uint64_t msi_addr_phys,
                  l4_uint64_t *msi_addr_iova) override
  {
    // Forward to platform provided MSI translator
    if (_platform_adapter)
      return _platform_adapter->map_msi_src(dev, msi_addr_phys, msi_addr_iova);
    else
      return -L4_ENODEV;
  }

  Dma_requester_id dma_alias() const override
  {
    // Root bridges don't create aliases
    return Dma_requester_id();
  }

  Bridge_if *parent_bridge() const override
  { return nullptr; }

public:
  explicit Root_bridge(unsigned segment, unsigned bus_nr, Hw::Device *host,
                       Platform_adapter_if *platform_adapter)
  : Bridge_base(bus_nr), _host(host), _platform_adapter(platform_adapter),
    _segment(segment)
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
                            Hw::Device *host, Platform_adapter_if *platform_adapter)
  : Root_bridge(segment, bus_nr, host, platform_adapter) {}

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

private:
  pthread_mutex_t _cfg_lock = PTHREAD_MUTEX_INITIALIZER;
};

struct Mmio_root_bridge : public Root_bridge
{
  explicit Mmio_root_bridge(unsigned segment, unsigned bus_nr,
                            Hw::Device *host,
                            l4_uint64_t phys_base, unsigned num_busses,
                            Platform_adapter_if *platform_adapter)
  : Root_bridge(segment, bus_nr, host, platform_adapter)
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
