/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "resource.h"
#include "irqs.h"

namespace Hw {

class Msi_resource : public Resource, public Msi_irq_pin
{
public:
  Msi_resource(unsigned msi)
  : Resource(Resource::Irq_res | Resource::Irq_type_falling_edge, msi, msi)
  {}

};


}
