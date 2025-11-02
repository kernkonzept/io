/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/l4int.h>
#include <vector>

namespace Hw {
  class Device;
}

#ifdef CONFIG_L4IO_PCI_SRIOV
class Whitelisted_sriov_device
{
public:
  Whitelisted_sriov_device(l4_uint16_t vendor_id, l4_uint16_t device_id)
  : vendor_id(vendor_id),
    device_id(device_id) {}
  l4_uint16_t vendor_id;
  l4_uint16_t device_id;

  friend bool
  operator==(Whitelisted_sriov_device const& lhs, Whitelisted_sriov_device const& rhs)
    {
      return lhs.vendor_id == rhs.vendor_id &&
        lhs.device_id == rhs.device_id;
    }
};
#endif

class Io_config
{
public:
  virtual bool transparent_msi(Hw::Device *) const = 0;
  virtual bool legacy_ide_resources(Hw::Device *) const = 0;
  virtual bool expansion_rom(Hw::Device *) const = 0;
  virtual int verbose() const = 0;
#ifdef CONFIG_L4IO_PCI_SRIOV
  virtual std::vector<Whitelisted_sriov_device> const* sriov_whitelist() const = 0;
#endif
  virtual ~Io_config() = 0;

  static Io_config *cfg;
};

inline Io_config::~Io_config() {}
