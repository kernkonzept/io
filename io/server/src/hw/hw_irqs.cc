#include "hw_irqs.h"
#include <map>
#include <memory>

namespace {

typedef std::map<unsigned, std::unique_ptr<Kernel_irq_pin>> Irq_map;
static Irq_map _real_irqs;

}


Io_irq_pin *
Hw::Irqs::real_irq(unsigned n)
{
  std::unique_ptr<Kernel_irq_pin> &r = _real_irqs[n];
  if (!r)
    r = std::make_unique<Kernel_irq_pin>(n);

  return r.get();
}



