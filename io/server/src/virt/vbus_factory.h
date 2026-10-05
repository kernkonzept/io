/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "main.h"
#include "debug.h"

#include <l4/cxx/avl_map>
#include <l4/cxx/hlist>
#include <string>
#include <typeinfo>

#include "hw/hw_device.h"
#include "type_matcher.h"
#include "vdevice.h"

namespace Vi {

class Feature_factory
: public Type_matcher<Feature_factory, std::shared_ptr<Vi::Dev_feature>>
{
public:
  virtual std::shared_ptr<Vi::Dev_feature>
  do_match(std::shared_ptr<Hw::Dev_feature> const &f) = 0;

protected:
  explicit Feature_factory(std::type_info const *type)
  : Type_matcher<Feature_factory, std::shared_ptr<Vi::Dev_feature>>(type) {}
};

class Resource_factory
: public Type_matcher<Resource_factory, std::shared_ptr<Resource>>
{
public:
  virtual std::shared_ptr<Resource>
  do_match(std::shared_ptr<Resource> const &r) = 0;

protected:
  explicit Resource_factory(std::type_info const *type)
  : Type_matcher<Resource_factory, std::shared_ptr<Resource>>(type) {}
};

class Dev_factory : public Type_matcher<Dev_factory, Device *>
{
public:
  virtual Device *vcreate() = 0;
  virtual Device *do_match(Hw::Device *f) = 0;

  typedef cxx::Avl_map<std::string, Dev_factory *> Name_map;

protected:
  explicit Dev_factory(std::type_info const *type)
  : Type_matcher<Dev_factory, Device *>(type)
  {}

  static Name_map &name_map()
  {
    static Name_map _name_map;
    return _name_map;
  }

public:
  static Device *create(std::string const &_class);
  static Device *create(Hw::Device *f)
  { return match(f); }

private:
  Dev_factory(Dev_factory const &);
  void operator = (Dev_factory const &);
};


template< typename VI, typename HW >
class Feature_factory_t : public Feature_factory
{
public:
  Feature_factory_t() : Feature_factory(&typeid(HW)) {}

  std::shared_ptr<Vi::Dev_feature>
  do_match(std::shared_ptr<Hw::Dev_feature> const &f) override
  {
    if (auto h = std::dynamic_pointer_cast<HW>(f))
      return std::make_shared<VI>(h);

    return nullptr;
  }
};

template< typename VI, typename HW >
class Resource_factory_t : public Resource_factory
{
public:
  Resource_factory_t() : Resource_factory(&typeid(HW)) {}

  std::shared_ptr<Resource>
  do_match(std::shared_ptr<Resource> const &r) override
  {
    if (auto h = std::dynamic_pointer_cast<HW>(r))
      return std::make_shared<VI>(h);

    return nullptr;
  }
};

template< typename V_DEV, typename HW_DEV = void >
class Dev_factory_t :  public Dev_factory
{
public:
  typedef HW_DEV Hw_dev;
  typedef V_DEV  V_dev;

  Dev_factory_t() : Dev_factory(&typeid(Hw_dev))
  { }


  Device *do_match(Hw::Device *dev) override
  {
    if (dev->ref_count())
      if (!dev->is_multi_vbus_dev())
        printf("WARNING: device '%s' already assigned to another virtual bus.\n",
               dev->name());

    if (!dynamic_cast<HW_DEV const*>(dev))
      return 0;

    return new V_dev(static_cast<Hw_dev*>(dev));
  }

  Device *vcreate() override
  { return 0; }

};

template< typename V_DEV >
class Dev_factory_t<V_DEV, void> :  public Dev_factory
{
public:
  typedef void  Hw_dev;
  typedef V_DEV V_dev;

  explicit Dev_factory_t(std::string const &_class) : Dev_factory(0)
  { name_map()[_class] = this; }


  Device *do_match(Hw::Device *) override
  { return 0; }

  Device *vcreate() override
  { return new V_dev; }

};


}

