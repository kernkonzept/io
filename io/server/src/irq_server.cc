/*
 * Copyright (C) 2014-2015, 2018-2020, 2023-2024 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_server_loop>

#include <pthread.h>
#include <pthread-l4.h>
#include <errno.h>
#include <memory>

#include "irq_server.h"
#include "debug.h"

namespace {

typedef L4Re::Util::Registry_server<> Irq_server;
static Irq_server *irq_server;

static void *_server_loop_func(void *_svr)
{
  Irq_server *svr = static_cast<Irq_server *>(_svr);
  svr->loop();
  return 0;
}

}

L4Re::Util::Object_registry *irq_queue()
{
  if (irq_server)
    return irq_server->registry();

  pthread_t irq_server_thread;
  int e = pthread_create(&irq_server_thread, NULL, NULL, NULL);
  if (e != 0)
    {
      d_printf(DBG_ERR, "fatal: could not create IRQ handler thread: %d\n",
               -errno);
      return 0;
    }

  auto server =
    std::make_unique<Irq_server>(Pthread::L4::cap(irq_server_thread),
                                 L4Re::Env::env()->factory());

  e = Pthread::L4::start(irq_server_thread, _server_loop_func, server.get());
  if (e < 0)
    {
      d_printf(DBG_ERR, "fatal: could not start IRQ handler thread: %d\n",
               e);
      return 0;
    }

  // The IRQ handler thread uses the server from now on, so it must stay alive.
  irq_server = server.release();
  d_printf(DBG_DEBUG, "created IRQ handler thread\n");
  return irq_server->registry();
}



