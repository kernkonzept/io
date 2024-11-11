/*
 * (c) 2009 Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>
#include <l4/vbus/vbus_types.h>

__BEGIN_DECLS

int L4_CV
l4vbus_mcspi_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                  unsigned channel, l4_umword_t *value);

int L4_CV
l4vbus_mcspi_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                   unsigned channel, l4_umword_t value);

__END_DECLS
