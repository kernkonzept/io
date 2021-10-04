/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 */

#include "scu_imx8qm.h"

namespace Hw {

void Scu_imx8qm::init()
{
  if (_initialized)
    return;

  _initialized = true;

  Hw::Device::init();

  set_name_if_empty("scu");

  Resource *regs = resources()->find("regs");
  if (!regs || regs->type() != Resource::Mmio_res)
    {
      d_printf(DBG_ERR, "error: %s: no base address set\n"
                        "       missing or wrong 'regs' resource\n", name());
      throw "scu init error";
    }

  l4_addr_t phys_base = regs->start();
  l4_addr_t size = regs->size();

  if (size < sizeof(l4_uint32_t) * 4)
    {
      d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx)\n", name(), size);
      throw "scu init error";
    }

  l4_addr_t vbase = res_map_iomem(phys_base, size);
  if (!vbase)
    {
      d_printf(DBG_ERR, "error: %s: cannot map registers (phys=%lx-%lx)\n",
               name(), phys_base, phys_base + size - 1);
      throw "scu init error";
    }

  d_printf(DBG_INFO, "%s: mapped %lx registers to %08lx\n",
           name(), phys_base, vbase);

  _mu = new Mu(vbase);

  for (size_t i = 0; i < _sids.size(); ++i)
    {
      d_printf(DBG_INFO, "%s: scu smmu: rscs: %u sid: %u\n",
               name(), _sids.rscs(i), _sids.sid(i));
      rm_set_master_sid(_sids.rscs(i), _sids.sid(i));
    }
}

static Hw::Device_factory_t<Scu_imx8qm> __hw_pf_factory("Scu_imx8qm");
}
