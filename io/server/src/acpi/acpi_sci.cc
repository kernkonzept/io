/*
 * Copyright (C) 2014-2020, 2024-2025 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "debug.h"
#include "irq_server.h"
#include "acpi_lib.h"

#include <memory>
#include <stdio.h>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/re/env>
#include "../io_acpi.h"

namespace {

class Acpi_sci :
  public Kernel_irq_pin,
  public L4::Irqep_t<Acpi_sci>
{
public:
  Acpi_sci(ACPI_OSD_HANDLER isr, void *context, int irqnum)
  : Kernel_irq_pin(irqnum), _isr(isr), _context(context) {}

  void handle_irq()
  {
    if (!_isr(_context))
      trace_event(TRACE_ACPI_EVENT, "SCI not handled\n");
    unmask();
  }

private:
  ACPI_OSD_HANDLER _isr;
  void *_context;
};

}

ACPI_STATUS
AcpiOsInstallInterruptHandler (
    UINT32                    interrupt_number,
    ACPI_OSD_HANDLER          service_routine,
    void                      *context)
{
  auto *env = L4Re::Env::env();
  int err;
  L4::Cap<L4::Icu> icu = env->get_cap<L4::Icu>("icu");
  if (!icu)
    {
      d_printf(DBG_ERR, "error: could not find ICU capability.\n");
      return AE_BAD_PARAMETER;
    }

  auto sci_owner = std::make_unique<Acpi_sci>(service_routine, context,
                                              interrupt_number);
  auto irq = L4Re::Util::make_shared_cap<L4::Irq>();
  if (!irq)
    {
      d_printf(DBG_ERR, "error: out of caps\n");
      return AE_NO_MEMORY;
    }
  if ((err = l4_error(env->factory()->create(irq.get()))) < 0)
    {
      d_printf(DBG_ERR, "error: could not create L4::Irq: %d\n", err);
      return AE_NO_MEMORY;
    }
  if (!irq_queue()->register_obj(sci_owner.get(), irq.get()).is_valid())
    {
      d_printf(DBG_ERR, "error: could not register ACPI event server\n");
      return AE_NO_MEMORY;
    }

  // The IRQ registry refers to the object from now on, so it must stay alive
  // even if the remaining setup fails.
  Acpi_sci *sci = sci_owner.release();

  if ((err = sci->bind(irq, L4_IRQ_F_NONE)) < 0)
    {
      d_printf(DBG_ERR, "error: irq bind failed: %d\n", err);
      return AE_BAD_PARAMETER;
    }

  d_printf(DBG_INFO, "created ACPI event server and attached it to irq %d\n",
           interrupt_number);

  sci->unmask();

  Hw::Acpi::register_sci(sci);

  return AE_OK;
};

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
    UINT32                   interrupt_number,
    ACPI_OSD_HANDLER         service_routine)
{
  printf("%s:%d:%s(%d, %p): UNINPLEMENTED\n",
         __FILE__, __LINE__, __func__,
         interrupt_number, service_routine);
  return AE_OK;
}

