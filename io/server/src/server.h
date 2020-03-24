/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/util/object_registry>

extern L4Re::Util::Object_registry *registry;

int server_loop();

namespace Internal {

static struct Io_svr_init
{ Io_svr_init(); } init;

};
