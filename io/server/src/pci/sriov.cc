/*
 * Copyright (C) 2020, 2024 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <pci-dev.h>
#include <pci-sriov.h>
#include <resource_provider.h>

#include <cstdio>
#include <unistd.h>

namespace Hw { namespace Pci {

Sr_iov_feature::Sr_iov_feature(Dev *dev, l4_uint16_t cap_ofs)
: _dev(dev), _cap_ofs(cap_ofs)
{
  auto cap = this->cap();
  auto initial_vfs = cap.read<Sr_iov_cap::Initial_vfs>();
  _total_vfs = cap.read<Sr_iov_cap::Total_vfs>();
  _num_vfs   = cap.read<Sr_iov_cap::Num_vfs>();
  _vf_offset = cap.read<Sr_iov_cap::Vf_offset>();
  _vf_stride = cap.read<Sr_iov_cap::Vf_stride>();

  d_printf(DBG_INFO, "== %04x:%02x:%02x.%x ================================\n",
           0, cap.addr().bus(), cap.addr().dev(), cap.addr().fn());
  d_printf(DBG_INFO,
           "found SR-IOV device: vfs(inital/total/num)=%d/%d/%d ofs=%d stride=%d\n",
           initial_vfs.v, _total_vfs.v, _num_vfs.v, _vf_offset.v, _vf_stride.v);

  // If VFs were already enabled, disable them before configuring the device.
  if (cap.read<Sr_iov_cap::Ctrl>().vf_enable())
    {
      d_printf(DBG_WARN, "SR-IOV: VFs were already enabled, disabling...\n");

      auto ctrl = cap.read<Sr_iov_cap::Ctrl>();
      ctrl.vf_enable() = 0;
      ctrl.vf_memory_enable() = 0;
      cap.write<Sr_iov_cap::Ctrl>(ctrl);

      // After VF Enable is cleared, no field in the SR-IOV capability may be
      // accessed until at least 1 s has elapsed.
      sleep(1);
    }

  // Configure ARI.
  Extended_cap ari = dev->find_ext_cap(Ari_cap::Id);
  if (ari.is_valid() && dev->bridge()->ari_forwarding_enabled())
    {
      auto ctrl = cap.read<Sr_iov_cap::Ctrl>();
      ctrl.ari_capable_hierarchy() = 1;
      cap.write(ctrl);
      _ari_capable = true;
    }

  // We do not support dependencies between PFs.
  auto fn_dep = cap.read<Sr_iov_cap::Fn_dep>();
  if (fn_dep.v != cap.addr().devfn())
    {
      d_printf(DBG_WARN, "SR-IOV: PF 0x%02x depends on other PF 0x%02x: disable\n",
               cap.addr().devfn(), fn_dep.v);
      return;
    }

  init_system_page_size();

  if (!discover_num_vfs())
    return;

  // Use the maximum possible number of VFs. From now on, NumVFs must not be
  // changed as we rely on VF Offset and VF Stride to remain stable.
  set_num_vfs(_total_vfs.v);

  discover_vbars();
}

void
Sr_iov_feature::set_num_vfs(l4_uint16_t num_vfs)
{
  // Configure desired number of VFs.
  _num_vfs.v = num_vfs;
  cap().write(_num_vfs);

  // VF offset and VF Stride may change if NumVFs is changed.
  _vf_offset = cap().read<Sr_iov_cap::Vf_offset>();
  _vf_stride = cap().read<Sr_iov_cap::Vf_stride>();
}

void
Sr_iov_feature::init_system_page_size()
{
  auto cap = this->cap();

  // The SR-IOV system page size (encoded as 2^12+n) controls the alignment of
  // VF BARs, so we must ensure it is at least equal to the page size of our
  // system (L4_PAGESIZE).
  constexpr unsigned min_ps = L4_PAGESIZE >> 12;

  auto system_ps = cap.read<Sr_iov_cap::System_ps>();
  if (min_ps > system_ps.v)
    {
      d_printf(DBG_WARN, "%04x:%02x:%02x.%x: "
               "SR-IOV.System Page Size too small: %u need %u\n",
               0, cap.addr().bus(), cap.addr().dev(), cap.addr().fn(),
               system_ps.v, min_ps);

      auto supported_ps = cap.read<Sr_iov_cap::Supported_ps>();
      while (system_ps.v < min_ps || !(system_ps.v & supported_ps.v))
        system_ps.v <<= 1;

      d_printf(DBG_INFO, "%04x:%02x:%02x.%x: "
               "set SR-IOV.System Page: %u\n",
               0, cap.addr().bus(), cap.addr().dev(), cap.addr().fn(),
               system_ps.v);

      cap.write(system_ps);
    }

  d_printf(DBG_INFO, "  supported_ps=%08x system_ps=%08x\n",
           cap.read<Sr_iov_cap::Supported_ps>().v,
           cap.read<Sr_iov_cap::System_ps>().v);
}

bool
Sr_iov_feature::discover_num_vfs()
{
  l4_uint16_t max_vfs = _total_vfs.v;
  char const *limit_reason;

  // Limit maximum number of VFs to the configured value.
  if (max_vfs > Max_vfs)
    {
      max_vfs = Max_vfs;
      limit_reason = "hardcoded";
    }

  // Determine maximum number of VFs we can enable.
  l4_uint32_t pf_addr = _dev->cfg_addr().devfn();
  for (l4_uint16_t num_vfs = 1; num_vfs < max_vfs; num_vfs++)
    {
      set_num_vfs(num_vfs);

      l4_uint32_t first_vf_addr = pf_addr + _vf_offset.v;
      l4_uint32_t last_vf_addr = first_vf_addr + num_vfs * _vf_stride.v;
      // We currently do not support allocating multiple bus numbers for a
      // SR-IOV device, so check that all VFs are on the same bus as the PF.
      // This limits the number of VFs we can support to 255 with ARI enabled,
      // or 7 when ARI is not supported.
      l4_uint32_t Bus_mask = _ari_capable ? ~0xffU : ~0x7U;
      if (   (first_vf_addr & Bus_mask) != (pf_addr & Bus_mask)
          || (last_vf_addr & Bus_mask) != (pf_addr & Bus_mask))
        {
          // VF on different bus than PF, so the num_vfs from the previous
          // iteration is the maximum we can support.
          max_vfs = num_vfs - 1;
          limit_reason = _ari_capable ? "bus resources limited"
                                      : "bus resources limited - no ARI";
          break;
        }
    }

  if (max_vfs == 0)
    {
      // Warn, unless we configured the maximum number of VFs as zero ourselves.
      if (Max_vfs > 0)
        d_printf(DBG_WARN, "SR-IOV: needs too much bus resources: disable\n");

      _total_vfs.v = 0;
      return false;
    }

  if (max_vfs < _total_vfs.v)
    {
      d_printf(DBG_WARN, "SR-IOV: limit number of VFs to: %u (%s)\n",
               max_vfs, limit_reason);
      _total_vfs.v = max_vfs;
    }

  return true;
}

void
Sr_iov_feature::discover_vbars()
{
  for (unsigned vbar_idx = 0; vbar_idx < 6; ++vbar_idx)
    {
      Cfg_bar vbar_cfg = cap() + Sr_iov_cap::Vf_bar0::Ofs + vbar_idx * 4;
      if (!vbar_cfg.parse())
        {
          _vbar[vbar_idx] = nullptr;
          continue;
        }

      d_printf(DBG_INFO, "  VBAR[%d]: %llx-%llx (%s-bit)\n",
               vbar_idx, vbar_cfg.base(), vbar_cfg.size(),
               vbar_cfg.is_64bit() ? "64" : "32");
      Resource_provider *vbar = new Resource_provider(Resource::Mmio_res
                                                      | Resource::Mem_type_rw
                                                      | Resource::F_hierarchical
                                                      | Resource::F_can_move
                                                      | Resource::F_internal);

      // 'vBA'n' as ID
      vbar->set_id(0x00414276 + (l4_uint32_t{'0' + vbar_idx} << 24));
      vbar->start_size(vbar_cfg.base(), vbar_cfg.size() * _total_vfs.v);
      vbar->alignment(vbar_cfg.size() - 1);

      _vbar[vbar_idx] = vbar;

      if (vbar_cfg.is_prefetchable())
        vbar->add_flags(Resource::F_prefetchable);

      if (vbar_cfg.is_64bit())
        {
          vbar->add_flags(Resource::F_width_64bit);
          _vbar[++vbar_idx] = nullptr;
        }

      vbar->validate();
      _dev->host()->add_resource_rq(vbar);
    }
}

void
Sr_iov_feature::init_vf(unsigned vf_idx, Sr_iov_cap::Vf_device_id vf_dev_id)
{
  // Calculate address of given VF relative to PF.
  l4_uint32_t relative_vf_addr = _vf_offset.v + (vf_idx * _vf_stride.v);
  l4_uint32_t vendor_device = (_dev->vendor_device_ids() & 0xffff)
                              | (l4_uint32_t{vf_dev_id.v} << 16);
  // The SR-IOV setup code for the PF only waits the minimum time of 100ms
  // required for us to be permitted to send configuration requests to the VF,
  // but after which the VF might still not yet be ready to process
  // configuration or memory requests. But fortunately in that case the Root
  // Complex will retry the configuration request until it completes with a
  // non-CRS status (see the "Configuration Request Retry Status" implementation
  // note in section "2.3.1 Request Handling Rules" of the "PCI Express Base
  // Specification Revision 5.0").
  Config_cache cfg;
  cfg.fill(vendor_device, _dev->config() + (relative_vf_addr << 12));

  // This is kind of a special HACKY case:
  // The device 'vf_dev' is placed as a sibling device of the PF below the same
  // bridge as the PF. However, the MMIO resources are placed as child resources
  // of the PF VBAR resources.

  Hw::Device *vf_dev = new Hw::Device(cfg.addr().devfn());
  Sr_iov_vf *vf = new Sr_iov_vf(vf_dev, _dev->bridge(), cfg);
  vf_dev->add_feature(vf);
  _dev->host()->parent()->add_child(vf_dev);

  for (unsigned bar_idx = 0; bar_idx < 6; ++bar_idx)
  {
    Resource_provider *vbar = get_vbar(bar_idx);
    if (!vbar)
      continue;

    Resource *bar = new Resource(Resource::Mmio_res
                                 | Resource::Mem_type_rw
                                 | Resource::F_hierarchical);

    // 'BAR'n' as ID
    bar->set_id(0x00524142 + (l4_uint32_t{'0' + bar_idx} << 24));
    l4_uint64_t size = vbar->size() / _total_vfs.v;
    l4_uint64_t start = vbar->start() + size * vf_idx;
    bar->start_size(start, size);
    bar->alignment(vbar->alignment());

    if (vbar->is_64bit())
      bar->add_flags(Resource::F_width_64bit);

    if (vbar->prefetchable())
      bar->add_flags(Resource::F_prefetchable);

    vf_dev->add_resource(bar);
    vbar->provided()->request(vbar, _dev->host(), bar, vf_dev);
    vf->set_bar(bar_idx, bar);
  }

  vf->discover_resources(vf_dev);
}

void
Sr_iov_feature::setup(Hw::Device *)
{
  d_printf(DBG_INFO, "setup SR-IOV device\n");
  if (_total_vfs.v == 0)
    {
      d_printf(DBG_WARN, "SR-IOV: no VFs possible\n");
      return;
    }

  if (!setup_vbars())
    return;

  auto ctrl = cap().read<Sr_iov_cap::Ctrl>();
  ctrl.vf_enable() = 1;
  ctrl.vf_memory_enable() = 1;
  cap().write(ctrl);

  // Wait the minimum time of 100ms required so that the subsequently executed
  // VF setup code is permitted to send configuration requests to the VFs. We
  // rely on the VF init code, see init_vf(), to make at least one configuration
  // request, so that on completion of that request we can be sure that the VF
  // is ready to process both configuration and memory requests. This allows to
  // avoid waiting for a whole second here, which would be required otherwise to
  // ensure that readiness (see section "9.3.3.3.1 VF Enable" of the "PCI
  // Express Base Specification Revision 5.0").
  l4_ipc_sleep_ms(100);

  // We can only initialize the VF devices now, after the PF set VF enable,
  // since only then the VF devices are realized by hardware.
  auto vf_dev_id = cap().read<Sr_iov_cap::Vf_device_id>();
  for (unsigned i = 0; i < _total_vfs.v; ++i)
    init_vf(i, vf_dev_id);

  _enabled = true;
  d_printf(DBG_INFO, "SR-IOV enabled...\n");
}

bool
Sr_iov_feature::setup_vbars()
{
  for (unsigned vbar_idx = 0; vbar_idx < 6; ++vbar_idx)
    {
      Resource *vbar = get_vbar(vbar_idx);
      if (!vbar)
        continue;

      if (vbar->disabled())
        {
          d_printf(DBG_ERR, "error: could not enable SR-IOV resource\n");
          return false;
        }

      if (vbar->empty())
        continue;

      Config vbar_cfg = cap() + Sr_iov_cap::Vf_bar0::Ofs + vbar_idx * 4;
      auto vbar_start = vbar->start();
      vbar_cfg.write<l4_uint32_t>(0, vbar_start);
      if (vbar->is_64bit())
        vbar_cfg.write<l4_uint32_t>(4, vbar_start >> 32);
    }

  return true;
}

void
Sr_iov_feature::dump(int indent) const
{
  printf("%*.sSR-IOV: (%s) vfs=%u ofs=%u stride=%u\n", indent, " ",
         _enabled ? "enabled" : "disabled",
         _num_vfs.v, _vf_offset.v, _vf_stride.v);
}

bool
Dev::handle_sriov_cap(Dev *dev, Extended_cap cap)
{
  Sr_iov_feature *sriov = new Sr_iov_feature(dev, cap.addr().reg());
  dev->host()->add_feature(sriov);
  return true;
}

} }
