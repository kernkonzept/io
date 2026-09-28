/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "debug.h"
#include "vpci.h"
#include "vpci_pci_bridge.h"
#include "vpci_proxy_dev.h"
#include "virt/vbus_factory.h"

#include <l4/vbus/vbus_pci-ops.h>

namespace Vi {

/**
 * \brief A virtual Host-to-PCI bridge.
 */
class Pci_vroot : public Vi::Pci_bridge
{
public:

  explicit Pci_vroot();

  char const *type_name() const override
  { return "virtual PCI root"; }

  ~Pci_vroot() {}

  char const *hid() const override
  { return "PNP0A03"; }

  Io_irq_pin::Msi_src *find_msi_src(Msi_src_info si) override;

private:
  /// The vBUS PCI root interface of the bridge, provided as device feature.
  class Pci_root_feature : public Vi::Dev_feature
  {
  public:
    explicit Pci_root_feature(Pci_vroot *root) : _root(root) {}

    int dispatch(l4_umword_t, l4_uint32_t func,
                 L4::Ipc::Iostream &ios) override;

    l4_uint32_t interface_type() const override
    { return 1 << L4VBUS_INTERFACE_PCI; }

    bool match_hw_feature(const Hw::Dev_feature*) const override
    { return false; }

    // The feature always belongs to its PCI root bridge.
    void set_host(Vi::Device *) override {}
    Vi::Device *host() const override { return _root; }

  private:
    Pci_vroot *_root;
  };

public:
  int cfg_read(L4::Ipc::Iostream &ios);
  int cfg_write(L4::Ipc::Iostream &ios);
  int irq_enable(L4::Ipc::Iostream &ios);
};

// -----------------------------------------------------------------------
// Virtual PCI root bridge with identity mapping of phys devices
// -----------------------------------------------------------------------


class Pci_vroot_id : public Vi::Pci_vroot
{
public:
  void add_child(std::unique_ptr<Vi::Device> d) override;

};

}



// -------------------------------------------------------------------
// Virtual PCI Root bridge
// -------------------------------------------------------------------

Vi::Pci_vroot::Pci_vroot() : Vi::Pci_bridge()
{
  add_feature(std::make_shared<Pci_root_feature>(this));
  _subordinate = 100;
}

Io_irq_pin::Msi_src *
Vi::Pci_vroot::find_msi_src(Msi_src_info si)
{
  if (si.query() == Msi_src_info::Query_requester_id) // exact match
    {
      Vi::Pci_dev *d = child_dev(si.bus(), si.dev(), si.fn());
      if (!d)
        return 0;
      return d->msi_src();
    }
  return 0;
}

int
Vi::Pci_vroot::cfg_read(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;
  l4_uint32_t reg;
  l4_uint32_t value = 0;
  l4_uint32_t width;

  ios >> bus >> devfn >> reg >> width;
  if (!Hw::Pci::cfg_width_valid(width))
    return -L4_EINVAL;

  value = ~0U >> (32 - width);

  if (0)
    printf("vPCI root: cfg read: %02x:%02x.%1x: reg=%x w=%d\n",
           bus, devfn >> 16, devfn & 0xffff, reg, width);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    {
      ios << value;
      return L4_EOK;
    }

  Vi::Pci_dev *d = child_dev(bus, (devfn >> 16), (devfn & 0xffff));

  if (!d)
    {
      ios << value;
      return L4_EOK;
    }

  int res = d->cfg_read(reg, &value, Hw::Pci::cfg_w_to_o(width));

  if (0)
    printf("  res=%d\n", res);

  if (res < 0)
    return res;

  if (0)
    printf("  value=%x\n", value);

  ios << value;
  return L4_EOK;
}

int
Vi::Pci_vroot::irq_enable(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;

  ios >> bus >> devfn;

  if (0)
    printf("irq enable: %02x:%02x.%1x\n", bus, devfn >> 16, devfn & 0xffff);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    {
      ios << (int)-1;
      return L4_EOK;
    }

  Vi::Pci_dev *d = child_dev(bus, devfn >> 16, devfn & 0xffff);
  if (0)
    printf("dev = %p\n", d);
  if (!d)
    {
      ios << (int)-1;
      return L4_EOK;
    }

  Vi::Pci_dev::Irq_info info;
  int res = d->irq_enable(&info);
  if (res < 0)
    {
      ios << (int)-1;
      return res;
    }

  if (0)
    printf("  irq = %d\n", info.irq);
  ios << info.irq << info.trigger << info.polarity;
  return L4_EOK;
}

int
Vi::Pci_vroot::cfg_write(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;
  l4_uint32_t reg;
  l4_uint32_t value;
  l4_uint32_t width;

  ios >> bus >> devfn >> reg >> value >> width;
  if (!Hw::Pci::cfg_width_valid(width))
    return -L4_EINVAL;

  if (0)
    printf("cfg write: %02x:%02x.%1x: reg=%x w=%x v=%08x\n",
           bus, devfn >> 16, devfn & 0xffff, reg, width, value);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    return L4_EOK;

  Vi::Pci_dev *d = child_dev(bus, (devfn >> 16), (devfn & 0xffff));

  if (!d)
    return L4_EOK;

  return d->cfg_write(reg, value, Hw::Pci::cfg_w_to_o(width));
}

int
Vi::Pci_vroot::Pci_root_feature::dispatch(l4_umword_t, l4_uint32_t func,
                                          L4::Ipc::Iostream &ios)
{
  if (l4vbus_subinterface(func) != L4VBUS_INTERFACE_PCI)
    return -L4_ENOSYS;

  switch (func)
    {
    case L4vbus_pciroot_cfg_read: return _root->cfg_read(ios);
    case L4vbus_pciroot_cfg_write: return _root->cfg_write(ios);
    case L4vbus_pciroot_cfg_irq_enable: return _root->irq_enable(ios);
    default: return -L4_ENOSYS;
    }
}


void
Vi::Pci_vroot_id::add_child(std::unique_ptr<Vi::Device> d)
{
  Vi::Pci_dev *vp = d->find_feature<Vi::Pci_dev>();

  // hm, also here, drop non PCI devices
  if (!vp)
    {
      d_printf(DBG_ERR, "warning: device %s is not a PCI device! Ignoring.\n",
               d->name());
      d.release();
      return;
    }

  if (Vi::Pci_proxy_dev *proxy = dynamic_cast<Vi::Pci_proxy_dev*>(vp))
    {
      unsigned dn = proxy->hwf()->device_nr() & (Bus::Devs-1);
      unsigned fn = proxy->hwf()->function_nr() & (Dev::Fns-1);
      unsigned bus_num = proxy->hwf()->bus_nr();

      if (bus_num == 0)
	{
	  // MUST be a device on the root PCI bus
	  _bus.dev(dn)->fn(fn, vp);
	  Vi::Device::add_child(std::move(d));
	  return;
	}

      Vi::Pci_bridge *sw_br = find_bridge(bus_num);
      if (!sw_br)
	{
	  auto b = std::make_unique<Vi::Pci_to_pci_bridge>();
	  sw_br = b.get();

	  b->primary(0);
	  b->secondary(bus_num);
	  b->subordinate(bus_num);

	  _bus.dev(31)->fn(0, b->function());
	  Vi::Device::add_child(std::move(b));
	}

      sw_br->add_child_fixed(std::move(d), vp, dn, fn);
      return;
    }
  else
    {
      _bus.add_fn(vp);
      Vi::Device::add_child(std::move(d));
    }
}



static Vi::Dev_factory_t<Vi::Pci_vroot> __pci_root_factory("PCI_bus");
static Vi::Dev_factory_t<Vi::Pci_vroot_id>
  __pci_root_id_factory("PCI_bus_ident");
