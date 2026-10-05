#pragma once

#include "resource.h"

namespace Hw {
  class Msi_resource;
}

namespace Vi {
  class Msi_resource : public Resource
  {
  private:
    std::shared_ptr<Hw::Msi_resource> _hw_msi;

  public:
    Msi_resource(std::shared_ptr<Hw::Msi_resource> const &hr);
    Hw::Msi_resource *hw_msi() const { return _hw_msi.get(); }
  };

}
