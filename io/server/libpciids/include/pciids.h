/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <sys/cdefs.h>
#include <l4/sys/compiler.h>

__BEGIN_DECLS

L4_EXPORT
void libpciids_name_device(char *name, int len,
                           unsigned vendor, unsigned device);

__END_DECLS
