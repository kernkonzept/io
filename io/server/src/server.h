/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/re/util/object_registry>

extern L4Re::Util::Object_registry *registry;

int server_loop();

namespace Internal {

static struct Io_svr_init
{ Io_svr_init(); } init;

};
