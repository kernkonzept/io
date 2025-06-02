/*
 * Copyright (C) 2025-2026 Kernkonzept GmbH.
 * Author(s): Jean Wolter <jean.wolter@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <l4/re/util/meta>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>

#include <l4/sys/factory>
#include <l4/sys/cxx/ipc_epiface>

#include "server.h"
#include "virt/vbus.h"
/**
 * The IPC interface for creating vbus caps.
 *
 * The IO factory provides an IPC interface to create vbus
 * endpoints. Vbus endpoints are the only option for a client to
 * communicate with the associated Vbus.
 *
 * The `IO_factory` gets constructed when IO starts. It thereafter
 * gets registered on the IO server to serve IPC `create` calls.
 */
class IO_factory : public L4::Epiface_t<IO_factory, L4::Factory>
{
public:
  /// Initialize and register factory
  IO_factory();

  /// Get a pointer to the IO server factory
  static IO_factory *get();

  /// Is the factory active?
  bool active()
  { return _active; }

  /**
   * Add Vbus to factory
   *
   * Add a VI::System_bus structure created by an IO config script to
   * the list of system buses managed known to the factory.
   *
   * \param vbus  Pointer to a Vi::System_bus structure
   */
  void add_vbus(Vi::System_bus *vbus);

  /**
   * Handle the create operation of the factory protocol
   *
   * Implementation of L4Re::Factory.create(). Creates an endpoint for
   * a vbus provided by IO if it is not bound to a capability.
   *
   * \param res[out]   Result capability.
   * \param type       Type of object to create. `type` has to be `0`.
   * \param vbus       Vararg string containing "vbus=<name>" to select the vbus
   *
   * \retval L4_EOK     Success.
   * \retval -L4_EINVAL Unknown `type` or remaining arguments not
   *                    understood/invalid.
   * \retval -L4_ENODEV Bus does not exist.
   * \retval -L4_EEXIST Bus is already associated with a capability.
   */
  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                 l4_umword_t type, L4::Ipc::Varg_list_ref va);
private:
  /*
   * Detect vanishing Vbus clients
   */
  struct Del_cap_irq : public L4::Irqep_t<Del_cap_irq>
  {
    // Forward deletion irqs to the IO factory
    void handle_irq()
    { _io_factory->handle_irq(); }

    Del_cap_irq(IO_factory *io_factory) : _io_factory{io_factory} {}

  private:
    IO_factory *_io_factory;
  };

  // Handle vanished Vbus clients
  void handle_irq();

  // Lookup Vbus
  Vi::System_bus *lookup_vbus(char const *opt_str, unsigned len);

  Del_cap_irq _del_cap_irq;
  std::vector<Vi::System_bus *> _busses;
  bool _active = false;
};
