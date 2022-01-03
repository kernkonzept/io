/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/sys/l4int.h>
#include <l4/sys/capability>

int res_init();

#if defined(ARCH_x86) || defined(ARCH_amd64)
int res_get_ioport(unsigned port, int size);
#else
static inline int res_get_ioport(unsigned, int) { return -L4_ENOSYS; }
#endif

l4_addr_t res_map_iomem(l4_uint64_t phys, l4_uint64_t size, bool cached = false);
