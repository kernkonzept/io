/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "iomuxc.h"

namespace Hw {

void
Iomuxc_imx8mp::init()
{
  if (_initialized)
    return;

  _initialized = true;

  Hw::Device::init();

  set_name_if_empty("Iomuxc_imx8mp");

  Resource *reg = resources()->find("reg0");
  if (!reg || reg->type() != Resource::Mmio_res)
    {
      d_printf(DBG_ERR, "error: %s: no base address set\n"
                        "       missing or wrong 'reg0' resource\n", name());
      throw "Iomuxc_imx8mp: missing or wrong reg0 resource.";
    }

  l4_addr_t phys_base = reg->start();
  l4_addr_t size = reg->size();

  if ((size & (size - 1)) != 0)
    {
      d_printf(DBG_ERR, "error: %s: unaligned MMIO resource size (0x%lx)\n",
               name(), size);
      throw "Iomuxc_imx8mp: unaligned MMIO resource size.";
    }

  l4_addr_t vbase = res_map_iomem(phys_base, size);
  if (!vbase)
    {
      d_printf(DBG_ERR, "error: %s: cannot map registers (phys=[%lx, %lx]\n",
               name(), phys_base, phys_base + size - 1);
      throw "Iomuxc_imx8mp: failed to map MMIO registers.";
    }

  d_printf(DBG_INFO, "%s: mapped 0x%lx registers to 0x%08lx\n",
           name(), phys_base, vbase);

  _mregs = cxx::make_unique<Mem_regs>(vbase, size);
}

static Hw::Device_factory_t<Iomuxc_imx8mp> __hw_pf_factory("Iomuxc_imx8mp");
}
