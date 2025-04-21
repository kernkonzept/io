/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "debug.h"
#include "hw_device.h"
#include "resource_provider.h"

#include <l4/cxx/unique_ptr>
#include <l4/drivers/hw_mmio_register_block>

#include <string>
#include <vector>

namespace Hw {

class Iomuxc_table_property : public Property
{
protected:
  std::vector<unsigned> _table;

public:
  int set(int, std::string const &) override { return -EINVAL; }
  int set(int, Generic_device *) override { return -EINVAL; }
  int set(int, Resource *) override { return -EINVAL; }

  int set(int, l4_int64_t i) override
  {
    _table.push_back(static_cast<unsigned>(i));
    return 0;
  }
};

class Mem_regs
{
public:
  Mem_regs(l4_addr_t base, l4_size_t size)
  : _base(base), _size(size)
  {}

  l4_uint32_t get_reg(l4_addr_t offset) const
  {
    if (!check_access(offset))
      return -1U;

    return _base.read<l4_uint32_t>(offset);
  }

  bool set_reg(l4_addr_t offset, l4_uint32_t value)
  {
    if (!check_access(offset))
      return false;

    _base.write<l4_uint32_t>(value, offset);
    return true;
  }

private:
  bool check_access(l4_addr_t offset) const
  {
    if (offset > _size)
      return false;

    if ((offset & 3) != 0)
      return false;

    return true;
  }

  L4drivers::Mmio_register_block<32> _base;
  l4_size_t _size;
};

/**
 * Description of a pad connection of the CPU.
 */
class Pad
{
public:
  Pad(l4_addr_t mux_offs, l4_addr_t cfg_offs, l4_addr_t input_offs,
      l4_uint32_t mux_val, l4_uint32_t cfg_val, l4_uint32_t input_val,
      Mem_regs *regs)
  : _mux_offs(mux_offs), _cfg_offs(cfg_offs), _input_offs(input_offs),
    _mux_val(mux_val), _cfg_val(cfg_val), _input_val(input_val),
    _regs(regs)
  {}

  bool config() { return configure(_mux_val, _cfg_val, _input_val); }

private:
  bool configure(l4_uint32_t mux_val, l4_uint32_t cfg_val, l4_uint32_t input_val)
  {
    d_printf(DBG_DEBUG, "IOMUXC PAD: Write to mux @0x%lx value 0x%x\n",
             _mux_offs, mux_val);
    bool ret = _regs->set_reg(_mux_offs, mux_val);
    if (!ret)
      return ret;

    d_printf(DBG_DEBUG, "IOMUXC PAD: Write to cfg @0x%lx value 0x%x\n",
             _cfg_offs, cfg_val);
    ret = _regs->set_reg(_cfg_offs, cfg_val);
    if (!ret)
      return ret;

    d_printf(DBG_DEBUG, "IOMUXC PAD: Write to input @0x%lx value 0x%x\n",
             _input_offs, input_val);
    ret = _regs->set_reg(_input_offs, input_val);

    return ret;
  }

  l4_addr_t _mux_offs;
  l4_addr_t _cfg_offs;
  l4_addr_t _input_offs;
  l4_uint32_t _mux_val;
  l4_uint32_t _cfg_val;
  l4_uint32_t _input_val;
  Mem_regs *_regs;
};


class Iomuxc_imx8mp : public Hw::Device
{
public:
  Iomuxc_imx8mp()
  {}

  void init() override;

  Mem_regs *mregs() const { return _mregs.get(); }

private:
  bool _initialized = false;
  cxx::unique_ptr<Mem_regs> _mregs;
};

template <typename DEV>
class Iomux_device : public DEV
{
  /**
   * Resource pads property
   */
  class Pads_property : public Iomuxc_table_property
  {
  public:
    size_t size() const
    { return _table.size() / 6; }

    unsigned mux(size_t idx) const
    { return _table.at(idx * 6); }

    unsigned cfg(size_t idx) const
    { return _table.at(idx * 6 + 1); }

    unsigned input(size_t idx) const
    { return _table.at(idx * 6 + 2); }

    unsigned mux_val(size_t idx) const
    { return _table.at(idx * 6 + 3); }

    unsigned cfg_val(size_t idx) const
    { return _table.at(idx * 6 + 5); }

    unsigned input_val(size_t idx) const
    { return _table.at(idx * 6 + 4); }
  };

public:
  Iomux_device()
  {
    this->register_property("iomuxc", &_iomuxc);
    this->register_property("pads", &_pads);
  }

  void init() override
  {
    if (_iomuxc.dev() == nullptr)
      {
        d_printf(DBG_ERR, "error: %s: 'iomuxc' not set.\n", DEV::name());
        throw "Iomuxc init error";
      }

    _iomuxc.dev()->init();

    d_printf(DBG_DEBUG, "Table size: %zu\n", _pads.size());

    for (unsigned i = 0; i < _pads.size(); ++i)
      {
        if (!pad_from_property(i).config())
          d_printf(DBG_WARN, "warning: %s: pad %i failed to configure.\n",
                   DEV::name(), i);
      }

    DEV::init();
  }

  Pad pad_from_property(unsigned idx)
  {
    if (idx > _pads.size())
      throw -L4_EINVAL;

    return Pad(_pads.mux(idx), _pads.cfg(idx), _pads.input(idx),
               _pads.mux_val(idx), _pads.cfg_val(idx), _pads.input_val(idx),
               _iomuxc.dev()->mregs());
  }

private:
  Device_property<Hw::Iomuxc_imx8mp> _iomuxc;
  Pads_property _pads;
};

} // namespace Hw

