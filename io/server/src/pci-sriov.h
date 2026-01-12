/*
 * Copyright (C) 2020, 2024-2025 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <pci-dev.h>
#include <pci-caps.h>
#include <resource_provider.h>

namespace Hw { namespace Pci {

/**
 * SR-IOV handler.
 *
 * Current limitations:
 *   - SR-IOV is always enabled if the device supports it.
 *   - Allocating multiple bus numbers for a SR-IOV device not supported.
 *   - Number of VFs to enable is configured statically according to the Max_vfs
 *     constant (limited by bus capacity and number of VFs the device supports).
 *   - Dependencies between PFs not supported.
 */
class Sr_iov_feature : public Dev_feature
{
public:
  // For now, hardcode the maximum number of virtual functions (VFs). In the
  // future configuring the number of VFs might be made dynamic, however that
  // would require preparatory work to make vbus more dynamic.
  static constexpr unsigned Max_vfs = CONFIG_L4IO_PCI_SRIOV_MAX_VFS;

  explicit Sr_iov_feature(Dev *dev, l4_uint16_t cap_ofs);

  Resource_provider *get_vbar(unsigned i) const
  {
    if (_vbar[i] && _vbar[i] != (Resource_provider *)1)
      return _vbar[i];

    return nullptr;
  }

  /// Enables SR-IOV, runs after discover and resource allocation phase.
  void setup(Hw::Device *host) override;

  void dump(int indent) const override;

private:
  Extended_cap cap() const { return _dev->config(_cap_ofs); }

  /// Write the NumVFs register in the device and update the local copies of
  /// the VF Offset and VF Stride registers.
  void set_num_vfs(l4_uint16_t num_vfs);

  /// Initialize the SR-IOV system page size register.
  void init_system_page_size();

  /// Discover the number of VFs the device supports and the number we can use.
  bool discover_num_vfs();

  /// Discover size and alignment of the PF's VBAR registers.
  void discover_vbars();

  /// Set up the PF's VBAR registers.
  bool setup_vbars();

  /// Initialize VF, i.e. create a Hw::Device and request resources.
  void init_vf(unsigned vf_index, Sr_iov_cap::Vf_device_id vf_dev_id);

private:
  Dev *_dev;
  l4_uint16_t _cap_ofs;
  Resource_provider *_vbar[6];

  Sr_iov_cap::Total_vfs _total_vfs;
  Sr_iov_cap::Num_vfs _num_vfs;
  Sr_iov_cap::Vf_offset _vf_offset;
  Sr_iov_cap::Vf_stride _vf_stride;
  bool _ari_capable = false;
  bool _enabled = false;
};

// We want to advertise the SR-IOV VF as a regular PCI device, but a VF differs
// in certain aspects from regular a PCI device. So it needs some special
// handling on top of Hw::Pci::Dev.
class Sr_iov_vf : public Dev
{
private:
  // VFs have no independent memory space enable flag, they are all linked to
  // VF memory enable in the PF. But we want to make it look like regular
  // PCI device, thus we emulate setting the Memory Space bit in the Command
  // register.
  bool _mse = true;

public:
  Sr_iov_vf(Hw::Device *host, Bridge_if *bridge, Config_cache const &cfg)
  : Dev(host, bridge, cfg)
  {}

  // TODO: We might want to not override this function, but instead just use the
  //       one from Hw::Pci::Dev. Would cause some extra unnecessary
  //       configuration space accesses, but all the features not implemented
  //       should be detected as such, so the result should be the same.
  void discover_resources(Hw::Device *host) override
  {
    if (flags.discovered())
     return;

    // No need to discover legacy IRQ, VFs do not support it.

    // No need to discover the BARs for VFs, they are RO-zero for VFs. The PF
    // has already set up the VF BARs for this VF and registered them via
    // Hw::Pci::Dev::set_bar().

    // No need to discover Expansion ROM, VFs do not support it.

    discover_pci_caps();

    // VFs can implement the ACS capability, so it is crucial to discover and
    // configure it to ensure the VF is isolated.
    Cap pcie = find_pci_cap(Cap::Pcie);
    if (pcie.is_valid())
        discover_pcie_caps();

    // Allocate DMA domain for VF.
    if (!host->dma_domain())
      host->parent()->dma_domain_for(host);

    flags.discovered() = true;
  }

  void setup(Hw::Device *) override
  {
    // No need to set up BARs or decoders, all VFs use the BAR configuration
    // provided by the PF.
  }

  l4_uint32_t recheck_bars(unsigned decoders) override
  {
    // For VFs, the values in BAR registers are RO Zero, so nothing to do here.
    return decoders;
  }

  l4_uint32_t checked_cmd_read() override
  {
    l4_uint32_t r = Dev::checked_cmd_read();
    if (_mse)
      r |= 2;
    return r;
  }

  l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) override
  {
    _mse = cmd & 2;
    return Dev::checked_cmd_write(mask, cmd);
  }
};

} }
