/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "vproxy_dev.h"
#include "vbus_factory.h"

Vi::Proxy_dev::Proxy_dev(Hw::Device *d)
: _hwd(d)
{
  d->add_client(this);
  // suck features from real dev
  for (auto const &i: *d->features())
    {
      std::shared_ptr<Vi::Dev_feature> vf = Vi::Feature_factory::match(i);
      if (vf)
        add_feature(vf);
    }

  // suck resources from our real dev
  for (auto i: *d->resources())
    {
      if (!i)
        continue;

      if (i->disabled() || i->internal())
        continue;

      std::shared_ptr<Resource> vr = Vi::Resource_factory::match(i);
      if (!vr)
        vr = i;

      add_resource(vr);
    }
}

namespace {
  static Vi::Dev_factory_t<Vi::Proxy_dev, Hw::Device> __ghwdf;
}
