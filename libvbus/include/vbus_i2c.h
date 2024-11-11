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
l4vbus_i2c_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                 l4_uint16_t addr, l4_uint8_t sub_addr,
                 l4_uint8_t *buffer, unsigned long size);

int L4_CV
l4vbus_i2c_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                l4_uint16_t addr, l4_uint8_t sub_addr,
                l4_uint8_t *buffer, unsigned long *size);

__END_DECLS
