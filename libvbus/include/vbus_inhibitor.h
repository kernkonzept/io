/**
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

enum Vbus_inhibitor
{
  L4VBUS_INHIBITOR_SUSPEND  = 0,
  L4VBUS_INHIBITOR_SHUTDOWN = 1,
  L4VBUS_INHIBITOR_REBOOT   = L4VBUS_INHIBITOR_SHUTDOWN,
  L4VBUS_INHIBITOR_WAKEUP   = 2,
  L4VBUS_INHIBITOR_MAX
};

