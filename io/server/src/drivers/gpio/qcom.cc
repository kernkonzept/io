/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */

/**
 * Driver for Qualcomm "Top-Level Mode Multiplexer" (TLMM) GPIO controller.
 *
 * \code{.lua}
 * TLMM = Hw.Gpio_qcom_chip(function()
 *   compatible = {"qcom,msm8916-pinctrl", "qcom,tlmm"};
 *   Resource.reg0 = Res.mmio(0x01000000, 0x010fffff);
 *   Resource.irq0 = Res.irq(32+208, Io.Resource.Irq_type_level_high);
 *   Property.ngpios = 122;           -- Total number of GPIOs
 *   Property.target_proc = 0x4;      -- Target processor for IRQs
 *   Property.reg_gpio_size = 0x1000; -- Size of a single GPIO register group
 * end);
 * \endcode
 */

#include <l4/drivers/hw_mmio_register_block>

#include "debug.h"
#include "gpio"
#include "hw_device.h"
#include "irqs.h"
#include "server.h"

#include "gpio_irq.h"

namespace {

enum Regs
{
  // Each GPIO is in a separate register group (size configured in _gpio_size)
  TLMM_GPIO_CFG         = 0x0,
  TLMM_GPIO_IN_OUT      = 0x4,
  TLMM_GPIO_INTR_CFG    = 0x8,
  TLMM_GPIO_INTR_STATUS = 0xc,
};

class Gpio_irq_pin : public Gpio_irq_base_t<Gpio_irq_pin>
{
private:
  enum Intr_cfg_regs {
    INTR_ENABLE            = 0x1 << 0,
    INTR_POL_SHIFT         = 1,
    INTR_POL_ACTIVE_LOW    = 0x0 << INTR_POL_SHIFT,
    INTR_POL_ACTIVE_HIGH   = 0x1 << INTR_POL_SHIFT,
    INTR_DECT_SHIFT        = 2,
    INTR_DECT_LEVEL        = 0x0 << INTR_DECT_SHIFT,
    INTR_DECT_POS_EDGE     = 0x1 << INTR_DECT_SHIFT,
    INTR_DECT_NEG_EDGE     = 0x2 << INTR_DECT_SHIFT,
    INTR_DECT_BOTH_EDGE    = 0x3 << INTR_DECT_SHIFT,
    INTR_RAW_STATUS_EN     = 0x1 << 4,
    INTR_TARGET_PROC_SHIFT = 5,
    INTR_TARGET_PROC_MASK  = 0b111,
  };

  L4drivers::Register_block<32> _regs;
  unsigned int _base;
  unsigned int _target_proc;

  void enable_intr(unsigned val)
  {
    val |= INTR_ENABLE | INTR_RAW_STATUS_EN | _target_proc;
    _regs[_base + TLMM_GPIO_INTR_CFG] = val;

    // Some interrupts seem to trigger once after enabling them
    _regs[_base + TLMM_GPIO_INTR_STATUS] = 0;
  }

public:
  Gpio_irq_pin(unsigned pin, L4drivers::Register_block<32> const &regs,
               unsigned base, unsigned target_proc)
  : Gpio_irq_base_t<Gpio_irq_pin>(pin), _regs(regs), _base(base),
    _target_proc((target_proc & INTR_TARGET_PROC_MASK) << INTR_TARGET_PROC_SHIFT)
  {}

  void do_mask()
  {
    _regs[_base + TLMM_GPIO_INTR_CFG].clear(INTR_ENABLE | INTR_RAW_STATUS_EN);
  }

  void do_unmask()
  {
    switch (mode())
      {
        case L4_IRQ_F_LEVEL_HIGH:
          enable_intr(INTR_DECT_LEVEL | INTR_POL_ACTIVE_HIGH);
          return;
        case L4_IRQ_F_LEVEL_LOW:
          enable_intr(INTR_DECT_LEVEL | INTR_POL_ACTIVE_LOW);
          return;
        case L4_IRQ_F_POS_EDGE:
          enable_intr(INTR_DECT_POS_EDGE | INTR_POL_ACTIVE_HIGH);
          return;
        case L4_IRQ_F_NEG_EDGE:
          enable_intr(INTR_DECT_NEG_EDGE | INTR_POL_ACTIVE_HIGH);
          return;
        case L4_IRQ_F_BOTH_EDGE:
          enable_intr(INTR_DECT_BOTH_EDGE | INTR_POL_ACTIVE_HIGH);
          return;
      }
  }

  bool do_set_mode(unsigned mode)
  {
    _mode = mode;
    if (enabled())
      do_unmask();
    return true;
  }

  bool handle_interrupt(bool mask_level)
  {
    // Check if the interupt is active
    if (!_regs[_base + TLMM_GPIO_INTR_STATUS])
      return false;

    // Mask level-triggered IRQs to avoid triggering them again immediately
    if (mask_level && mode() & L4_IRQ_F_LEVEL)
      do_mask();

    // Clear the interrupt
    _regs[_base + TLMM_GPIO_INTR_STATUS] = 0;
    return true;
  }

  int clear() override
  {
    return Io_irq_pin::clear() + handle_interrupt(false);
  }
};

class Gpio_irq_server : public Irq_demux_t<Gpio_irq_server>
{
private:
  unsigned _npins;

public:
  Gpio_irq_server(unsigned irq, unsigned npins)
  : Irq_demux_t<Gpio_irq_server>(irq, 0, npins), _npins(npins)
  { enable(); }

  void handle_irq()
  {
    for (unsigned pin = 0; pin < _npins; pin++)
      {
        if (!_pins[pin] || !_pins[pin]->enabled())
          continue;
        if (dynamic_cast<Gpio_irq_pin*>(_pins[pin])->handle_interrupt(true))
          _pins[pin]->trigger();
      }
    enable();
  }
};

class Gpio_qcom_chip : public Hw::Gpio_device
{
private:
  enum Gpio_cfg_regs
  {
    GPIO_PULL_SHIFT = 0,
    GPIO_PULL_NONE  = 0x0 << GPIO_PULL_SHIFT,
    GPIO_PULL_DOWN  = 0x1 << GPIO_PULL_SHIFT,
    GPIO_PULL_KEEP  = 0x2 << GPIO_PULL_SHIFT,
    GPIO_PULL_UP    = 0x3 << GPIO_PULL_SHIFT,
    GPIO_PULL_MASK  = 0x3 << GPIO_PULL_SHIFT,
    GPIO_FUNC_SHIFT = 2,
    GPIO_FUNC_MASK  = 0xf << GPIO_FUNC_SHIFT,
    GPIO_OE         = 1 << 9,
  };

  enum Gpio_in_out_regs
  {
    GPIO_IN  = 1 << 0,
    GPIO_OUT = 1 << 1,
  };

  L4drivers::Register_block<32> _regs;
  Gpio_irq_server *_irq_svr;

  Int_property _ngpios;
  Int_property _target_proc;
  Int_property _reg_gpio_size;

  unsigned pin_reg(unsigned reg, unsigned pin)
  { return reg + (pin * _reg_gpio_size); }

public:
  Gpio_qcom_chip() : _target_proc(~0)
  {
    register_property("ngpios", &_ngpios);
    register_property("target_proc", &_target_proc);
    register_property("reg_gpio_size", &_reg_gpio_size);
  }

  unsigned nr_pins() const override { return _ngpios; }
  void setup(unsigned pin, unsigned mode, int value = 0) override;
  void config_pull(unsigned pin, unsigned mode) override;
  int get(unsigned pin) override;
  void set(unsigned pin, int value) override;
  Io_irq_pin *get_irq(unsigned pin) override;

  void init() override;

  void multi_setup(Pin_slice const &mask, unsigned mode,
                   unsigned outvalues = 0) override
  { generic_multi_setup(mask, mode, outvalues); }
  void multi_config_pad(Pin_slice const &mask, unsigned func,
                        unsigned value) override
  { generic_multi_config_pad(mask, func, value); }
  void multi_set(Pin_slice const &mask, unsigned data) override
  { generic_multi_set(mask, data); }
  unsigned multi_get(unsigned offset) override
  { return generic_multi_get(offset); }

  // Platform-specific configuration is not supported at the moment
  void config_get(unsigned, unsigned, unsigned *) override
  { throw -L4_EINVAL; }
  void config_pad(unsigned, unsigned, unsigned) override
  { throw -L4_EINVAL; }
};

void
Gpio_qcom_chip::setup(unsigned pin, unsigned mode, int value)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  switch (mode)
    {
      case Input:
      case Irq: // No special IRQ mode
        mode = 0;
        break;
      case Output:
        mode = GPIO_OE;
        break;
      default:
        // although setup is part of the generic Gpio API
        // we allow hardware specific modes as well
        mode = (mode << GPIO_FUNC_SHIFT) & GPIO_FUNC_MASK;
        break;
    }

  _regs[pin_reg(TLMM_GPIO_CFG, pin)].modify(GPIO_OE | GPIO_FUNC_MASK, mode);

  if (mode == GPIO_OE)
    set(pin, value);
}

void
Gpio_qcom_chip::config_pull(unsigned pin, unsigned mode)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  switch (mode)
    {
      case Pull_none:
        mode = GPIO_PULL_NONE;
        break;
      case Pull_up:
        mode = GPIO_PULL_UP;
        break;
      case Pull_down:
        mode = GPIO_PULL_DOWN;
        break;
      default:
        d_printf(DBG_WARN, "warning: %s: invalid PUD mode for pin %u. "
                           "Ignoring.\n", name(), pin);
        throw -L4_EINVAL;
    }

  _regs[pin_reg(TLMM_GPIO_CFG, pin)].modify(GPIO_PULL_MASK, mode);
}

int
Gpio_qcom_chip::get(unsigned pin)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  return _regs[pin_reg(TLMM_GPIO_IN_OUT, pin)] & GPIO_IN;
}

void
Gpio_qcom_chip::set(unsigned pin, int value)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  _regs[pin_reg(TLMM_GPIO_IN_OUT, pin)] = value ? GPIO_OUT : 0;
}

Io_irq_pin *
Gpio_qcom_chip::get_irq(unsigned pin)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  if (!_irq_svr)
    return nullptr;

  return _irq_svr->get_pin<Gpio_irq_pin>(pin, _regs, pin_reg(0, pin),
                                         _target_proc);
}

void
Gpio_qcom_chip::init()
{
  Gpio_device::init();

  if (   assert_property(&_ngpios, "ngpios", 0)
      || assert_property(&_target_proc, "target_proc", ~0)
      || assert_property(&_reg_gpio_size, "reg_gpio_size", 0))
    return;

  Resource *regs = resources()->find("reg0");
  if (!regs || regs->type() != Resource::Mmio_res)
    {
      d_printf(DBG_ERR, "error: %s: no base address set for device: Gpio_qcom_chip\n"
                        "       missing or wrong 'regs' resource\n"
                        "       the chip will not work at all!\n", name());
      return;
    }

  l4_addr_t phys_base = regs->start();
  l4_addr_t size = regs->size();

  auto end = pin_reg(0, nr_pins());
  if (size < end)
    {
      d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx) for device: Gpio_qcom_chip\n"
                        "       the chip will not work at all!\n", name(), size);
      return;
    }

  l4_addr_t vbase = res_map_iomem(phys_base, size);
  if (!vbase)
    {
      d_printf(DBG_ERR, "error: %s: cannot map registers for Gpio_qcom_chip\n"
                        "       phys=%lx-%lx\n",
               name(), phys_base, phys_base + size - 1);
      return;
    }

  d_printf(DBG_DEBUG2, "%s: Gpio_qcom_chip: mapped registers to %08lx\n",
           name(), vbase);

  _regs = new L4drivers::Mmio_register_block<32>(vbase);

  Resource *irq = resources()->find("irq0");
  if (irq && irq->type() == Resource::Irq_res)
    _irq_svr = new Gpio_irq_server(irq->start(), nr_pins());
  else
    d_printf(DBG_WARN, "warning: %s: Gpio_qcom_chip no irq configured\n", name());
}

static Hw::Device_factory_t<Gpio_qcom_chip> __hw_gpio_qcom_factory("Gpio_qcom_chip");

}
