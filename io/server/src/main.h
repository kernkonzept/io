/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/capability>
#include <l4/sys/icu>

namespace Hw {
class Device;
}

class Hw_icu
{
public:
  L4::Cap<L4::Icu> icu;
  L4::Icu::Info info;

  Hw_icu();
};

Hw::Device *system_bus();
Hw_icu *system_icu();

