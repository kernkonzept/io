/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/vbus/vbus_types.h>
#include <l4/sys/types.h>

/**
 * \ingroup l4vbus_module
 * \defgroup l4vbus_pm_module L4vbus power management functions
 * \{
 */

__BEGIN_DECLS

/**
 * \copybrief L4vbus::Pm::pm_suspend()
 * \param vbus      V-BUS capability
 * \param handle    Handle for the device to be suspended
 * \copydetails L4vbus::Pm::pm_suspend()
 */
int L4_CV
l4vbus_pm_suspend(l4_cap_idx_t vbus, l4vbus_device_handle_t handle);

/**
 * \copybrief L4vbus::Pm::pm_resume()
 * \param vbus      V-BUS capability
 * \param handle    Handle for the device to be resumed
 * \copydetails L4vbus::Pm::pm_resume()
 */
int L4_CV
l4vbus_pm_resume(l4_cap_idx_t vbus, l4vbus_device_handle_t handle);

/**\}*/

__END_DECLS
