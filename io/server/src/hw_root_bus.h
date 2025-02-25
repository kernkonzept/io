/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "hw_device.h"

#include <functional>
#include <l4/cxx/unique_ptr>

namespace Hw {

class Root_bus : public Device
{
public:
  struct Pm
  {
    virtual int suspend() = 0;
    virtual int shutdown() = 0;
    virtual int reboot() = 0;
    virtual ~Pm() = 0;
  };

  explicit Root_bus(char const *name);

  void set_pm(cxx::unique_ptr<Pm>&& pm)
  {
    _pm = cxx::move(pm);
  }

  /// Test if power management API is supported
  bool supports_pm() const { return _pm; }

  void suspend();

  /**
   * \pre supports_pm() must be true
   */
  void shutdown() { _pm->shutdown(); }

  /**
   * \pre supports_pm() must be true
   */
  void reboot() { _pm->reboot(); }

  using Resource_cb = std::function<bool(Resource const *r)>;

  void set_can_alloc_cb(Resource_cb cb) { _can_alloc_cb = cb; }

  bool can_alloc_from_res(Resource const *r) override
  {
    if (_can_alloc_cb)
      return _can_alloc_cb(r);

    return true;
  }

private:
  cxx::unique_ptr<Pm> _pm;

  std::function<bool(Resource const *r)> _can_alloc_cb;
};

inline Root_bus::Pm::~Pm() {}

}
