/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/l4int.h>
#include <l4/vbus/vbus_types.h>
#include <l4/cxx/minmax>
#include <l4/cxx/string>
#include <l4/cxx/bitfield>

#include "resource.h"
#include "debug.h"

#include <cstdio> 
#include <cstring>
#include <string>
#include <map>

template<typename D> class Device_tree_mixin;

/**
 * The device tree consists of nodes with links to their parent, child and
 * sibling nodes. If there is no parent node, this node is a root node.
 * After setup, there must only be one root node.
 *
 * The parent has only one pointer to a child node.
 * Sibling nodes have the same parent node and the same depth in the tree.
 * The siblings form a single-linked list via their next pointer.
 * The head of this single-linked list is the child pointer in the parent node.
 *
 * The depth describes the number of parent nodes until the root node is
 * reached.
 * The root node has depth zero.
 */
template< typename D >
class Device_tree
{
  friend class Device_tree_mixin<D>;

private:
  D *_n;        ///< next sibling node
  D *_p;        ///< parent node
  D *_c;        ///< child node
  int _depth;   ///< depth of this node

public:
  Device_tree() : _n(0), _p(0), _c(0), _depth(0) {}

  D *parent() const { return _p; }
  D *children() const { return _c; }
  D *next() const { return _n; }
  int depth() const { return _depth; }

private:
  /**
   * Set \a p as parent device
   *
   * \param p Parent device
   */
  void set_parent(D *p) { _p = p; }

  void add_sibling(D *s)
  { _n = s; }

  /**
   * Add \a d as child device to \a self
   *
   * \param d     Child device
   * \param self  Parent device
   */
  void add_child(D *d, D *self)
  {
    if (d->parent())
      {
         d_printf(DBG_ERR, "warning: device %s already has a parent. Ignoring.\n",
                  d->name());
         return;
      }

    for (iterator i = iterator(0, d, L4VBUS_MAX_DEPTH); i != iterator(); ++i)
      i->set_depth(i->depth() + depth() + 1);

    d->set_parent(self);

    if (!_c)
      _c = d;
    else
      {
        D *p;
        for (p = _c; p->next(); p = p->next())
          ;
        p->add_sibling(d);
      }
  }

  void set_depth(int d) { _depth = d; }

public:
  class iterator
  {
  public:
    /**
     * Construct an iterator for a subtree of devices.
     *
     * \param p      Root node of the subtree to iterate.
     * \param c      Node to start iterating from.
     * \param depth  Maximum depth of the iteratable subtree relative to `p`.
     */
    iterator(D *p, D *c, int depth = 0)
    : _p(p), _c(c), _d(depth + (p ? p->depth() : 0))
    {}

    /**
     * Construct an iterator for a subtree of devices with root node `p`.
     *
     * \param p      Root node of the subtree to iterate.
     * \param depth  Maximum depth of the iteratable subtree relative to `p`.
     *
     * \pre `p` must not be nullptr.
     */
    iterator(D const *p, int depth = 0)
    : _p(p), _c(p->children()), _d(depth + p->depth())
    {}

    /// Construct an invalid interator.
    iterator()
    : _c(0)
    {}

    bool operator == (iterator const &i) const
    {
      if (!_c && !i._c)
        return true;

      return _p == i._p && _c == i._c && _d == i._d;
    }

    bool operator != (iterator const &i) const
    { return !operator == (i); }

    D *operator -> () const { return _c; }
    D *operator * () const { return _c; }

    /// Advance to next device; if there is none, return an invalid iterator.
    iterator operator ++ ()
    {
      if (!_c)
        return *this;

      // This performs a limited-depth, depth-first search algorithm.

      if (_d > _c->depth() && _c->children())
        // go to a child if not at max depth and there are children
        _c = _c->children();
      else if (_c->next())
        // go to the next sibling
        _c = _c->next();
      else if (_c == _p)
        _c = 0;
      else
        {
          for (D *x = _c->parent(); x && x != _p; x = x->parent())
            if (x->next())
              {
                _c = x->next();
                return *this;
              }
          _c = 0;
        }

      return *this;
    }

    iterator operator ++ (int)
    {
      iterator o = *this;
      ++(*this);
      return o;
    }

  private:
    D const *_p;  ///< parent device
    D *_c;        ///< current device or end()
    int _d;       ///< max depth to iterate up to
  };
};

template< typename D >
class Device_tree_mixin
{
  friend class Device_tree<D>;

protected:
  Device_tree<D> _dt;

public:
  typedef typename Device_tree<D>::iterator iterator;

  iterator begin(int depth = 0) const
  { return iterator(static_cast<D const*>(this), depth); }

  static iterator end() { return iterator(); }

  D *find_by_name(char const *name) const
  {
    for (iterator c = begin(0); c != end(); ++c)
      if (strcmp((*c)->name(), name) == 0)
        return *c;

    return 0;
  }

  virtual void add_child(D *c) { _dt.add_child(c, static_cast<D*>(this)); }
  virtual ~Device_tree_mixin() {}

private:
  void set_depth(int d) { return _dt.set_depth(d); }
  void set_parent(D *p) { _dt.set_parent(p); }
  void add_sibling(D *s) { _dt.add_sibling(s); }
};

class Resource_container
{
public:
  virtual Resource_list const *resources() const = 0;
  virtual bool resource_allocated(Resource const *) const  = 0;
  virtual ~Resource_container() {}
};


class Device : public Resource_container
{
public:
  struct Msi_src_info
  {
    l4_uint64_t v = 0;
    Msi_src_info(l4_uint64_t v) : v(v) {}
    CXX_BITFIELD_MEMBER(63, 63, is_dev_handle, v);
    CXX_BITFIELD_MEMBER( 0, 62, dev_handle, v);


    CXX_BITFIELD_MEMBER(18, 19, svt, v);
    CXX_BITFIELD_MEMBER(16, 17, sq, v);

    CXX_BITFIELD_MEMBER( 0, 15, sid, v);

    CXX_BITFIELD_MEMBER( 8, 15, bus, v);
    CXX_BITFIELD_MEMBER( 3,  7, dev, v);
    CXX_BITFIELD_MEMBER( 0,  2, fn, v);
    CXX_BITFIELD_MEMBER( 0,  7, devfn, v);

    CXX_BITFIELD_MEMBER( 8, 15, start_bus, v);
    CXX_BITFIELD_MEMBER( 0,  7, end_bus, v);
  };

  virtual Device *parent() const = 0;
  virtual Device *children() const = 0;
  virtual Device *next() const = 0;
  virtual int depth() const = 0;

  bool request_child_resource(Resource *, Device *);
  bool alloc_child_resource(Resource *, Device *);

  void request_resource(Resource *r);
  void request_resources();
  void request_child_resources();
  void allocate_pending_child_resources();
  void allocate_pending_resources();

  virtual char const *name() const = 0;
  virtual char const *hid() const = 0;

  virtual void dump(int) const {};

  typedef Device_tree<Device>::iterator iterator;

  iterator begin(int depth = 0) const { return iterator(this, depth); }
  static iterator end() { return iterator(); }

  virtual int pm_suspend() = 0;
  virtual int pm_resume() = 0;

  virtual std::string get_full_path() const = 0;
};

class Property;

class Generic_device : public Device
{
private:
  typedef std::map<std::string, Property *> Property_list;
  Resource_list _resources;
  Property_list _properties;

public:
  //typedef gen_iterator<Generic_device> iterator;

  Resource_list const *resources() const override { return &_resources; }
  void add_resource(Resource *r)
  { _resources.push_back(r); }

  void add_resource_rq(Resource *r)
  {
    add_resource(r);
    request_resource(r);
  }

  virtual bool match_cid(cxx::String const &) const { return false; }

  char const *name() const override { return "(noname)"; }
  char const *hid() const override { return 0; }

  int pm_suspend() override { return 0; }
  int pm_resume() override { return 0; }

  std::string get_full_path() const override;

  /**
   * Register a property
   *
   * Register a property so that it can be set via Lua. The name of the
   * property has to be unique. Hardware device drivers use the properties
   * 'hid' and 'adr' by default.
   *
   * \param name  Name of the property
   * \param prop  Property object which should be registered
   * \return 0 if the property has been registered, -EEXIST if there already
   *         is a property with the same name
   */
  int register_property(std::string const &name, Property *prop);

  /**
   * Retrieve property object
   *
   * Retrieve the property object which corresponds with \a name
   *
   * \param name  Name of the requested property
   * \return The corresponding property object if it exists, NULL otherwise
   */
  Property *property(std::string const &name);
};

/**
 * Base class to define device driver properties
 *
 * This class is not intended to be used directly. Instead you should inherit
 * from this class to define your own property type if necessary.
 */
class Property
{
public:
  Property() = default;
  Property &operator = (Property const &) = delete;
  Property(Property const &) = delete;

  virtual ~Property() = 0;

  /**
   * Set property value
   *
   * \param k   Array index in the case of an array property (starting from 1)
   *            and -1 in the case of a single value property.
   * \param val Property value
   */
  virtual int set(int k, std::string const &str) = 0;

  /**
   * \copydoc set(int k, std::string const &val) = 0;
   */
  virtual int set(int k, l4_int64_t val) = 0;

  /**
   * \copydoc set(int k, std::string const &val) = 0;
   */
  virtual int set(int k, Generic_device *val) = 0;

  /**
   * \copydoc set(int k, std::string const &val) = 0;
   */
  virtual int set(int k, Resource *val) = 0;
};

inline Property::~Property() {}

/// This class implements a string property
class String_property : public Property
{
private:
  std::string _s;

public:
  int set(int k, std::string const &str) override
  {
    if (k != -1)
      return -EINVAL;

    _s = str;
    return 0;
  }

  int set(int, l4_int64_t) override { return -EINVAL; }
  int set(int, Generic_device *) override { return -EINVAL; }
  int set(int, Resource *) override { return -EINVAL; }

  /// Read property value
  std::string const &val() const { return _s; }
};

/**
 * This class implements an integer property
 *
 * The value of this property is stored as a 64-bit signed integer
 */
class Int_property : public Property
{
private:
  l4_int64_t _i = 0;

public:
  Int_property() = default;
  Int_property(l4_int64_t i) : _i(i)
  {}

  operator l4_int64_t () const { return _i; }

  int set(int, std::string const &) override { return -EINVAL; }

  int set(int k, l4_int64_t i) override
  {
    if (k != -1)
      return -EINVAL;

    _i = i;
    return 0;
  }

  int set(int, Generic_device *) override { return -EINVAL; }
  int set(int, Resource *) override { return -EINVAL; }

  l4_int64_t val() const { return _i; }
};

/// This class implements a device reference property
template<typename DEVICE>
class Device_property : public Property
{
private:
  DEVICE *_dev;

public:
  Device_property(): _dev(0) {}

  int set(int, std::string const &) override { return -EINVAL; }
  int set(int, l4_int64_t) override { return -EINVAL; }
  int set(int k, Generic_device *d) override
  {
    if (k != -1)
      return -EINVAL;

    _dev = dynamic_cast<DEVICE *>(d);
    if (!_dev)
      return -EINVAL;

    return 0;
  }
  int set(int, Resource *) override { return -EINVAL; }

  DEVICE *dev() { return _dev; }
};

/// This class implements a resource property
class Resource_property : public Property
{
private:
  Resource *_res;

public:
  Resource_property() : _res(0) {}

  int set(int, std::string const &) override { return -EINVAL; }
  int set(int, l4_int64_t) override { return -EINVAL; }
  int set(int, Generic_device *) override { return -EINVAL; }
  int set(int k, Resource *r) override
  {
    if (k != -1)
      return -EINVAL;

    _res = r;

    return 0;
  }

  Resource *res() { return _res; }
};

