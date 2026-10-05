/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <set>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/consts>
#include <l4/cxx/hlist>

#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_interfaces.h>
#include <l4/vbus/vbus>
#include <l4/re/util/event_svr>
#include <l4/re/util/event_buffer>

#include <mutex>
#include <limits.h>

#include "vdevice.h"
#include "device.h"
#include "inhibitor_mux.h"
#include "dma_domain.h"

namespace Vi {
class Sw_icu;

struct Vbus_event_source : L4Re::Util::Event_svr<Vbus_event_source>
{
  typedef L4Re::Util::Event_buffer::Event Event;
  L4Re::Util::Event_buffer buffer;
  std::mutex lock;

  void reset_event_buffer()
  {
    std::lock_guard<std::mutex> g(lock);
    buffer.reset();
  }

  virtual L4::Epiface::Server_iface *server_iface() const = 0;

  Vbus_event_source();

  bool put(Event const &ev, bool trigger = true)
  {
    bool res;
      {
        std::lock_guard<std::mutex> g(lock);
        res = buffer.put(ev);
      }

    if (res && trigger)
      _irq.trigger();
    return res;
  }

  virtual int get_stream_info_for_id(l4_umword_t, L4Re::Event_stream_info *) = 0;
  virtual int get_stream_state_for_id(l4_umword_t, L4Re::Event_stream_state *) = 0;
};

class System_bus :
  public Device,
  public L4::Epiface_t<System_bus, L4vbus::Vbus>,
  public Inhibitor_provider,
  public Vbus_event_source
{
public:
  char const *type_name() const override
  { return "virtual system bus"; }

  // Factory to create root resource objects
  struct Root_resource_factory : cxx::H_list_item_t<Root_resource_factory>
  {
    virtual std::shared_ptr<Root_resource> create(System_bus *bus) const = 0;
    Root_resource_factory()
    { _factories.push_front(this); }

    typedef cxx::H_list_t<Root_resource_factory> Factory_list;
    static Factory_list _factories;
  };

  // template to create a factory for root objects of type
  // \a TYPE.
  template< unsigned TYPE, typename RS >
  struct Root_resource_factory_t : Root_resource_factory
  {
    std::shared_ptr<Root_resource> create(System_bus *bus) const override
    {
      return std::make_shared<Root_resource>(TYPE, std::make_shared<RS>(bus));
    }
  };

  // comparison operation to sort resources
  struct Res_cmp
  {
    // Allow lookups by plain resource pointers.
    using is_transparent = void;

    bool operator () (Resource const *a, Resource const *b) const
    {
      if (a->type() == b->type())
        return a->lt_compare(b);
      return a->type() < b->type();
    }

    bool operator () (std::shared_ptr<Resource> const &a,
                      std::shared_ptr<Resource> const &b) const
    { return (*this)(a.get(), b.get()); }

    bool operator () (std::shared_ptr<Resource> const &a,
                      Resource const *b) const
    { return (*this)(a.get(), b); }

    bool operator () (Resource const *a,
                      std::shared_ptr<Resource> const &b) const
    { return (*this)(a, b.get()); }
  };

  // Set of all resources of all types
  typedef std::set<std::shared_ptr<Resource>, Res_cmp> Resource_set;


  // Vi::System_bus interface
  explicit System_bus(Inhibitor_mux *hw_bus);
  ~System_bus() noexcept;

  void dump_resources() const;
  bool add_resource_to_bus(std::shared_ptr<Resource> const &r);
  Resource_set const *resource_set() const { return &_resources; }
  Sw_icu *sw_icu() const { return _sw_icu; }
  void sw_icu(Sw_icu *icu) { _sw_icu = icu; }
  void finalize();

  char const *inhibitor_name() const override
  { return Device::name(); }

  // L4::Server_object interface
  int op_clear(L4Re::Dataspace::Rights,
               L4Re::Dataspace::Offset,
               L4Re::Dataspace::Size)
  {
    return -L4_ENOSYS;
  }

  using Vbus_event_source::op_info;
  int op_info(L4Re::Dataspace::Rights, L4Re::Dataspace::Stats &stats)
  {
    stats.size = L4::trunc_page(ULONG_MAX);
    stats.flags = L4Re::Dataspace::F::RW;
    return 0;
  }

  int op_allocate(L4Re::Dataspace::Rights,
                  L4Re::Dataspace::Offset,
                  L4Re::Dataspace::Size)
  {
    return 0;
  }

  int op_copy_in(L4Re::Dataspace::Rights,
                 L4Re::Dataspace::Offset,
                 L4::Ipc::Snd_fpage,
                 L4Re::Dataspace::Offset,
                 L4Re::Dataspace::Size)
  {
    return -L4_ENOSYS;
  }

  int op_map(L4Re::Dataspace::Rights r,
             L4Re::Dataspace::Offset offset,
             L4Re::Dataspace::Map_addr spot,
             L4Re::Dataspace::Flags flags,
             L4::Ipc::Snd_fpage &fp);

  long op_map_info(L4Re::Dataspace::Rights r,
                   l4_addr_t &start_addr,
                   l4_addr_t &end_addr);

  long op_acquire(L4Re::Inhibitor::Rights r, l4_umword_t id,
                  L4::Ipc::String<> reason);

  long op_release(L4Re::Inhibitor::Rights r, l4_umword_t id);

  long op_next_lock_info(L4Re::Inhibitor::Rights r, l4_mword_t &id,
                         L4::Ipc::String<char> &name);

  l4_msgtag_t op_dispatch(l4_utcb_t *utcb, l4_msgtag_t tag, L4vbus::Vbus::Rights r);

  Server_iface *server_iface() const override
  { return L4::Epiface::server_iface(); }


  int pm_suspend() override;
  int pm_resume() override;


  // Device interface
  bool resource_allocated(Resource const *) const override;


  // Pm_client interface
  void pm_notify(unsigned, unsigned) {}

  Io_irq_pin::Msi_src *find_msi_src(Msi_src_info si) override;

  bool dev_notify(Device const *dev, unsigned type,
                  unsigned event, unsigned value, bool syn) override;

  void inhibitor_signal(l4_umword_t id) override;

  void reset_dma_domains();

private:
  /// The vBUS interface of the system bus, provided as device feature.
  class Vbus_feature : public Dev_feature
  {
  public:
    explicit Vbus_feature(System_bus *bus) : _bus(bus) {}

    l4_uint32_t interface_type() const override
    { return 1 << L4VBUS_INTERFACE_BUS; }

    int dispatch(l4_umword_t, l4_uint32_t func,
                 L4::Ipc::Iostream &ios) override;
    bool match_hw_feature(Hw::Dev_feature const *) const override
    { return false; }

    // The feature always belongs to its system bus.
    void set_host(Device *) override {}
    Device *host() const override { return _bus; }

  private:
    System_bus *_bus;
  };

  /// All DMA domains on the vBUS, used for the ~0U DMA domain id.
  class Dma_domains : public Dma_domain_set
  {
  public:
    explicit Dma_domains(System_bus const *bus) : _bus(bus) {}

    Domains domains() const override;

  private:
    System_bus const *_bus;
  };

  int request_resource(L4::Ipc::Iostream &ios);
  int assign_dma_domain(L4::Ipc::Iostream &ios);

  int get_stream_info_for_id(l4_umword_t, L4Re::Event_stream_info *) override;
  int get_stream_state_for_id(l4_umword_t, L4Re::Event_stream_state *) override;

  Device *dev_from_id(l4vbus_device_handle_t dev, int err) const;
  iterator rpc_get_dev_next_iterator(Device *dev, L4::Ipc::Iostream &ios, int err) const;
  int rpc_get_next_dev(Device *dev, L4::Ipc::Iostream &ios, int err) const;
  int rpc_get_dev_by_hid(Device *dev, L4::Ipc::Iostream &ios) const;
  int rpc_device_get(Device *dev, L4::Ipc::Iostream &ios) const;

  int dispatch_generic(L4vbus::Vbus::Rights obj, Device *dev, l4_uint32_t func, L4::Ipc::Iostream &ios);

  Resource_set _resources;
  Sw_icu *_sw_icu;
  Int_property _num_msis;
  Dma_domains _dma_domains{this};
  std::vector<Device *> _devices_by_id;
};

}
