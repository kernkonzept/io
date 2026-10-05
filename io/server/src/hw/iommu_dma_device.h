/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "hw_device.h"
#include "iommu.h"

namespace Hw
{

class Iommus_property : public Property
{
public:
  int set(int, std::string const &) override { return -EINVAL; }

  // TODO: Maybe check that all mappings were initialized properly (count set()
  //       calls, ensure k increments by 1 between each call and the number of
  //       calls is a multiple of Num_cells).

  int set(int k, l4_int64_t val) override
  {
    if (k < 1 || val < 0)
      return -EINVAL;

    // lua tables start at index 1
    unsigned index = k - 1;
    if ((index % Num_cells) != 1)
      return -EINVAL;

    unsigned device_id_index = index / Num_cells;
    if (device_id_index >= _interfaces.size())
      return -EINVAL;
    _interfaces[device_id_index].device_id = val;
    return 0;
  }
  int set(int k, Generic_device *val) override
  {
    if (k < 1)
      return -EINVAL;

    // lua tables start at index 1
    unsigned index = k - 1;
    if ((index % Num_cells) != 0)
      return -EINVAL;

    Iommu *iommu = dynamic_cast<Iommu *>(val);
    if (!iommu)
      return -EINVAL;

    unsigned device_id_index = index / Num_cells;
    if (device_id_index >= _interfaces.size())
      _interfaces.resize(device_id_index + 1);
    _interfaces[device_id_index].iommu = iommu;
    return 0;
  }

  int set(int, Resource *) override { return -EINVAL; }

  int enumerate_dma_src_ids(Dma_src_feature::Dma_src_id_cb cb) const
  {
    for (auto &interface : _interfaces)
      {
        l4_uint64_t src_id = interface.iommu->translate_device_id(
          interface.device_id);
        if (int err = cb(src_id); err < 0)
          return err;
      }
    return 0;
  }

private:
  static constexpr unsigned Num_cells = 2;

  struct Dma_interface
  {
    Iommu *iommu;
    l4_uint64_t device_id;
  };

  std::vector<Dma_interface> _interfaces;
};

/**
 * Hw::Device with specific properties for IOMMUs.
 *
 * This class offers the "iommus" property that specifies the DMA device ID(s)
 * of non-PCI platform-specific devices behind an IOMMU.
 *
 * Take the following Linux device tree entry:
 * \code{.dts}
 *  dma_device {
 *      iommus = <&iommu2 42>;
 *  };
 * \endcode
 *
 * This entry could be converted into the following io.cfg code:
 * \code{.lua}
 *  dma_device = Hw.Iommu_dma_device(function()
 *      Property.iommus = { iommu2, 42 };
 *  end);
 * \endcode
 *
 * The iommus table holds 2 values for each DMA device ID. The
 * individual elements are:
 *   1. Reference to a Hw.Iommu device
 *   2. Device ID
 * We always assume #iommu-cells = 1.
 * In case the iommu node in your Devicetree has zero cells, just pass a
 * placeholder of 0 as Device ID.
 * In case the iommu node in your Devicetree has two cells, combine the two
 * 32-bit cells into one 64-bit Device ID.
 */
class Iommu_dma_device : public Hw::Device, public Dma_src_feature
{
public:
  Iommu_dma_device()
  {
    register_property("iommus", &_iommus);
    property("flags")->set(-1, DF_dma_supported);

    add_feature(static_cast<Dma_src_feature *>(this));
  }

  int enumerate_dma_src_ids(Dma_src_feature::Dma_src_id_cb cb) const override
  {
    return _iommus.enumerate_dma_src_ids(cb);
  }

  int enumerate_dma_reservations(Dma_domain_if::Resv_cb) const override
  {
    return 0;
  }

private:
  Iommus_property _iommus;
};
} // namespace Hw
