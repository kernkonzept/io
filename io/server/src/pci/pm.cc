/*
 * (c) 2013-2020 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <pci-dev.h>
#include <pci-caps.h>

bool
Hw::Pci::Dev::check_pme_status()
{
  if (!cfg.pm_cap)
    return false;

  auto pm = config(cfg.pm_cap);
  Pm_cap::Pmcsr pmcsr = pm.read<Pm_cap::Pmcsr>();

  if (!pmcsr.pme_status())
    return false;

  bool res = false;
  // clear PME# status flag
  pmcsr.pme_status() = 1;
  if (pmcsr.pme_enable())
    {
      pmcsr.pme_enable() = 0;
      res = true;
    }

  pm.write(pmcsr);
  return res;
}


