/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

enum Debug_level
{
  DBG_NONE  = 0,
  DBG_ERR,
  DBG_WARN,
  DBG_INFO,
  DBG_DEBUG,
  DBG_DEBUG2,
  DBG_ALL
};

void set_debug_level(unsigned level);
void d_printf(unsigned level, char const *fmt, ...)
  __attribute__((format(printf, 2, 3)));
bool dlevel(unsigned level);

enum Trace_events
{
  TRACE_ACPI_EVENT = 0x1,
};

void set_trace_mask(unsigned mask);
void trace_event(unsigned event, char const *fmt, ...)
  __attribute__((format(printf, 2, 3)));
bool trace_event_enabled(unsigned event);

void acpi_set_debug_level(unsigned level);

