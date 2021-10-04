/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 */

#pragma once

#include "debug.h"
#include "hw_device.h"
#include "resource_provider.h"
#include <l4/drivers/hw_mmio_register_block>

namespace Hw {

class Scu_table_property : public Property
{
protected:
  std::vector<unsigned> _table;

public:
  int set(int, std::string const &) override { return -EINVAL; }
  int set(int, Generic_device *) override { return -EINVAL; }
  int set(int, Resource *) override { return -EINVAL; }

  int set(int, l4_int64_t i) override
  {
    _table.push_back((unsigned)i);
    return 0;
  }
};

class Scu_imx8qm : public Hw::Device
{
public:
  enum
  {
    Pm_pw_mode_off  = 0U, /* Power off */
    Pm_pw_mode_stby = 1U, /* Power in standby */
    Pm_pw_mode_lp   = 2U, /* Power in low-power */
    Pm_pw_mode_on   = 3U, /* Power on */
  };

  Scu_imx8qm()
  { register_property("sids", &_sids); }

  void init() override;

  int rm_set_master_sid(l4_uint16_t res, l4_uint16_t sid)
  {
    init(); // this may be called before init was called

    Scu_msg msg(Svc_rm, Func_rm_set_master_sid, 2);
    msg.data.r<16>(0) = res;
    msg.data.r<16>(2) = sid;
    scu_call(&msg, true);
    return msg.data.r<8>(0);
  }

  int pm_set_resource_power_mode(l4_uint16_t res, l4_uint8_t mode)
  {
    init(); // this may be called before init was called

    Scu_msg msg(Svc_pm, Func_pm_set_resource_power_mode, 2);
    msg.data.r<16>(0) = res;
    msg.data.r<8>(2)  = mode;
    scu_call(&msg, true);
    return msg.data.r<8>(0);
  }

  int pm_clock_enable(l4_uint16_t res, l4_uint8_t clk, bool enable, bool autog)
  {
    init(); // this may be called before init was called

    Scu_msg msg(Svc_pm, Func_pm_clock_enable, 3);
    msg.data.r<16>(0) = res;
    msg.data.r<8>(2) = clk;
    msg.data.r<8>(3) = enable;
    msg.data.r<8>(4) = autog;
    scu_call(&msg, true);
    return msg.data.r<8>(0);
  }

  int pad_set(l4_uint16_t pad, l4_uint32_t mux, l4_uint32_t conf)
  {
    init(); // this may be called before init was called

    Scu_msg msg(Svc_pad, Func_pad_set, 3);
    msg.data.r<32>(0) = (mux << 27) | conf;
    msg.data.r<16>(4) = pad;
    scu_call(&msg, true);
    return msg.data.r<8>(0);
  }

private:
  /**
   * Smmu sids property
   *
   * Pairs of rscs and sids to use, e.g:
   *
   *  Property.sids = {
   *    248, 1, -- IMX_SC_R_SDHC_0 // emmc
   *    249, 1, -- IMX_SC_R_SDHC_1 // sdcard
   *    250, 2, -- IMX_SC_R_SDHC_2 // wifi
   *  }
   */
  class Sids_property : public Scu_table_property
  {
  public:
    size_t size() const
    { return _table.size() / 2; }

    int rscs(size_t idx) const
    { return _table.at(idx * 2); }

    int sid(size_t idx) const
    { return _table.at(idx * 2 + 1); }
  };

  class Mu
  {
    enum
    {
      MU_SR_TE0_MASK1 = 1 << 23,
      MU_SR_RF0_MASK1 = 1 << 27,
      MU_ATR0_OFFSET1 = 0x0,
      MU_ARR0_OFFSET1 = 0x10,
      MU_ASR_OFFSET1  = 0x20,
      MU_ACR_OFFSET1  = 0x24
    };

  public:
    Mu(l4_addr_t vbase)
    { _mu = new L4drivers::Mmio_register_block<32>(vbase); }

    void write_mu(unsigned int regIndex, l4_uint32_t msg)
    {
      unsigned int mask = MU_SR_TE0_MASK1 >> regIndex;

      // Wait for TX to be empty
      while (!(_mu[MU_ASR_OFFSET1] & mask))
        ;
      _mu[MU_ATR0_OFFSET1  + (regIndex * 4)] = msg;
    }

    void read_mu(unsigned int regIndex, l4_uint32_t *msg)
    {
      unsigned int mask = MU_SR_RF0_MASK1 >> regIndex;

      // Wait for RX to be full
      while (!(_mu[MU_ASR_OFFSET1] & mask))
        ;
      *msg = _mu[MU_ARR0_OFFSET1 + (regIndex * 4)];
    }

  private:
    L4drivers::Register_block<32> _mu;
  };

  enum
  {
    Svc_pm = 2U,
    Svc_rm = 3U,
    Svc_pad = 6U
  };
  enum
  {
    Func_pm_set_resource_power_mode = 3U,
    Func_pm_clock_enable = 7U,
  };
  enum { Func_rm_set_master_sid = 11U };
  enum { Func_pad_set = 15U };

  struct Scu_msg
  {
    union
    {
      struct
      {
        l4_uint8_t ver;
        l4_uint8_t size;
        l4_uint8_t svc;
        l4_uint8_t func;
      } h;
      l4_uint32_t hdr;
    };
    // data block
    l4_uint32_t d[4];
    // view on data block
    L4drivers::Register_block<32> data{
      new L4drivers::Mmio_register_block<32>((l4_addr_t)d)};

    explicit Scu_msg(l4_uint8_t svc, l4_uint8_t func, l4_uint8_t sz)
    {
      h.ver = 1;
      h.svc = svc;
      h.size = sz;
      h.func = func;
    }
  };

  void scu_call(Scu_msg *msg, bool has_result)
  {
    // Write header
    _mu->write_mu(0, msg->hdr);
    unsigned count = 1;

    // Write data into the 4 channels
    while (count < msg->h.size)
      {
        _mu->write_mu(count % 4 , msg->d[count - 1]);
        count++;
      }

    if (has_result)
      {
        // Read header
        _mu->read_mu(0, &msg->hdr);
        count = 1;

        // Read data from the 4 channels
        while (count < msg->h.size)
          {
            _mu->read_mu(count % 4, &msg->d[count - 1]);
            count++;
          }
      }
  }

  bool _initialized = false;
  Sids_property _sids;
  Mu *_mu = nullptr;
};

template<class DEV>
class Scu_device : public DEV
{
public:
  /**
   * Resource power property
   *
   * Rscs to use, e.g:
   *
   *  Property.power = { 100 }; -- IMX_SC_R_I2C_4
   */
  class Scu_power_property : public Scu_table_property
  {
  public:
    size_t size() const
    { return _table.size(); }

    unsigned pin(size_t idx) const
    { return _table.at(idx); }
  };

  /**
   * Resource clocks property
   *
   * Pairs of rscs and clocks to use, e.g:
   *
   *  Property.clks = { 100, 2 }; -- IMX_SC_R_I2C_4, IMX_SC_PM_CLK_PER
   */
  class Scu_clks_property : public Scu_table_property
  {
  public:
    size_t size() const
    { return _table.size() / 2; }

    unsigned res(size_t idx) const
    { return _table.at(idx * 2); }

    unsigned clk(size_t idx) const
    { return _table.at(idx * 2 + 1); }
  };

  /**
   * Resource pads property
   *
   * Pairs of pads, mux and conf to use, e.g:
   *
   *  Property.pads =
   *     { 169, 1, 0xc600004c,   -- IMX8QM_ENET1_MDIO_DMA_I2C4_SDA
   *       170, 1, 0xc600004c }; -- IMX8QM_ENET1_MDC_DMA_I2C4_SCL
   */
  class Scu_pads_property : public Scu_table_property
  {
  public:
    size_t size() const
    { return _table.size() / 3; }

    unsigned pad(size_t idx) const
    { return _table.at(idx * 3); }

    unsigned mux(size_t idx) const
    { return _table.at(idx * 3 + 1); }

    unsigned conf(size_t idx) const
    { return _table.at(idx * 3 + 2); }
  };

  Scu_device()
  {
    this->register_property("scu", &_scu);
    this->register_property("power", &_power);
    this->register_property("pads", &_pads);
    this->register_property("clks", &_clks);
  }

  void init() override
  {
    if (_scu.dev() == nullptr)
      {
        d_printf(DBG_ERR, "error: %s: 'scu' not set.\n", DEV::name());
        throw "Scu_device init error";
      }

    for (size_t i = 0; i < _power.size(); ++i)
      _scu.dev()->pm_set_resource_power_mode(_power.pin(i),
                                             Hw::Scu_imx8qm::Pm_pw_mode_on);
    for (size_t i = 0; i < _clks.size(); ++i)
      _scu.dev()->pm_clock_enable(_clks.res(i), _clks.clk(i), true, false);

    for (size_t i = 0; i < _pads.size(); ++i)
      _scu.dev()->pad_set(_pads.pad(i), _pads.mux(i), _pads.conf(i));

    DEV::init();
  }

protected:
  Device_property<Hw::Scu_imx8qm> _scu;
  Scu_power_property _power;
  Scu_pads_property _pads;
  Scu_clks_property _clks;
};

}
