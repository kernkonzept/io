/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam.lackorzynski@kernkonzept.com>
 *            Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 */

#include "debug.h"
#include "irqs.h"
#include "gpio"
#include "hw_device.h"
#include "hw_irqs.h"
#include "gpio_irq.h"
#include "../scu_imx8qm.h"

#include <cstdio>

#include <l4/util/util.h>
#include <l4/drivers/hw_mmio_register_block>
#include "resource_provider.h"

namespace {

typedef L4drivers::Register_block<32> Chipregs;

enum
{
  GPIO_DR       = 0x00,
  GPIO_GDIR     = 0x04,
  GPIO_PSR      = 0x08,
  GPIO_ICR1     = 0x0c,
  GPIO_ICR2     = 0x10,
  GPIO_IMR      = 0x14,
  GPIO_ISR      = 0x18,
  GPIO_EDGE_SEL = 0x1c,
};

class Irq_pin : public Gpio_irq_base_t<Irq_pin>
{
public:
  Irq_pin(unsigned vpin, Chipregs const &regs)
  : Gpio_irq_base_t<Irq_pin>(vpin), _regs(regs)
  {}

  void do_mask()
  {
    _regs[GPIO_IMR].clear(1 << pin());
  }

  void do_unmask()
  {
    _regs[GPIO_IMR].set(1 << pin());
  }

  bool do_set_mode(unsigned mode)
  {
    _mode = mode;

    if (mode == L4_IRQ_F_BOTH_EDGE)
      _regs[GPIO_EDGE_SEL].set(1 << pin());
    else
      {
        _regs[GPIO_EDGE_SEL].clear(1 << pin());

        unsigned r = pin() >= 16 ? GPIO_ICR2 : GPIO_ICR1;
        unsigned p = (pin() % 16) * 2;

        switch (mode)
          {
          case L4_IRQ_F_NEG_EDGE:
            _regs[r].modify(3 << p, 3 << p);
            break;
          case L4_IRQ_F_POS_EDGE:
            _regs[r].modify(3 << p, 2 << p);
            break;
          case L4_IRQ_F_LEVEL_HIGH:
            _regs[r].modify(3 << p, 1 << p);
            break;
          case L4_IRQ_F_LEVEL_LOW:
            _regs[r].modify(3 << p, 0 << p);
            break;
          }
      }

    return true;
  }

private:

  Chipregs _regs;
};

class Irq_server : public Irq_demux_t<Irq_server>
{
public:
  Irq_server(int irq, unsigned flags, Chipregs const &regs)
  : Irq_demux_t<Irq_server>(irq,
                            (flags & Resource::Irq_type_mask)
                            / Resource::Irq_type_base,
                            32),
    _regs(regs)
  {
    enable();
  }

  void handle_irq_both()
  {
    l4_uint32_t isr = _regs[GPIO_ISR] & _regs[GPIO_IMR];

    while (isr)
      {
        unsigned p = ffs(isr) - 1;

        Gpio_irq_base *po = _pins[p];
        if (!po)
          printf("Wrong pin %d got an interrupt\n", p);
        else
          po->trigger();

        isr &= ~(1 << p);
      }
  }

  void handle_irq()
  {
    handle_irq_both();
  }

private:
  Chipregs _regs;
};

// This is just forwarding interrupts of the second interrupt to the
// Irq_server handler
class Irq_server_secondary : public Irq_demux_t<Irq_server_secondary>
{
public:
  Irq_server_secondary(int irq, unsigned flags, Irq_server *irq_svr)
  : Irq_demux_t<Irq_server_secondary>(irq,
                                      (flags & Resource::Irq_type_mask)
                                      / Resource::Irq_type_base,
                                      0),
    _irq_svr(irq_svr)
  {
    enable();
  }

  void handle_irq()
  {
    _irq_svr->handle_irq_both();
  }

private:
  Irq_server *_irq_svr;
};

class Gpio_imx_chip :  public Hw::Gpio_device
{
public:
  Gpio_imx_chip()
  {
    add_cid("gpio");
    add_cid("gpio-imx35");
  }

  void init() override;

  void request(unsigned) override {}
  void free(unsigned) override {}
  void setup(unsigned pin, unsigned mode, int outvalue = 0) override;
  int  get(unsigned pin) override;
  void set(unsigned pin, int value) override;
  void config_pad(unsigned pin, unsigned func, unsigned value) override;
  void config_get(unsigned pin, unsigned func, unsigned *value) override;
  Io_irq_pin *get_irq(unsigned pin) override;

  void multi_setup(Pin_slice const &mask, unsigned mode, unsigned outvalues) override;
  void multi_config_pad(Pin_slice const &mask, unsigned func, unsigned value) override;

  void multi_set(Pin_slice const &mask, unsigned data) override
  {
    unsigned m = mask.mask << mask.offset;
    _regs[GPIO_DR].modify(m, m & (data << mask.offset));
  }

  void config_pull(unsigned pin, unsigned mode) override;

  unsigned multi_get(unsigned offset) override
  {
    return _regs[GPIO_DR] >> offset;
  }

  int set_power_state(unsigned, bool)
  {
    return -L4_ENOSYS;
  }

  unsigned nr_pins() const override
  {
    return 32;
  }

private:
  Chipregs _regs;
  Irq_server *_irq_svr;
  Irq_server_secondary *_irq_svr_secondary = nullptr;
};

void
Gpio_imx_chip::setup(unsigned pin, unsigned mode, int value)
{
  printf("%s: setup(%d, mode=%u, value=%d)\n", name(), pin, mode, value);

  switch (mode)
    {
    case Input:
      _regs[GPIO_GDIR].clear(1 << pin);
      break;
    case Output:
      _regs[GPIO_GDIR].set(1 << pin);
      set(pin, value);
      break;
    default:
      break;
    }
}

int
Gpio_imx_chip::get(unsigned pin)
{
  int v = (_regs[GPIO_DR] >> pin) & 1;
  return v;
}

void
Gpio_imx_chip::set(unsigned pin, int value)
{
  _regs[GPIO_DR].modify(1 << pin, (!!value) << pin);
}

void
Gpio_imx_chip::config_pad(unsigned pin, unsigned func, unsigned value)
{
  switch (func)
    {
    case GPIO_DR:
    case GPIO_GDIR:
      _regs[func].modify(1 << pin, (value & 1) << pin);
      break;
    case GPIO_PSR:
      throw -L4_EINVAL;
    case GPIO_ICR1:
    case GPIO_ICR2:
      if (pin > 15)
        throw -L4_EINVAL;
      _regs[func].modify(3 << (pin * 2), (value & 3) << (pin * 2));
      break;
    case GPIO_IMR:
    case GPIO_EDGE_SEL:
      _regs[func].modify(1 << pin, (value & 1) << pin);
      break;
    case GPIO_ISR: // w1c -- write 1 to clear
      _regs[func] = (value & 1) << pin;

      if (pin >= 16 || !_irq_svr_secondary)
        _irq_svr->enable();
      else
        _irq_svr_secondary->enable();
      break;
    default:
      throw -L4_EINVAL;
    }
}

static l4_uint32_t bitmask_blow_16_to_32(l4_uint32_t m)
{
  unsigned m2 = 0;
  for (unsigned i = 0; i < 16; ++i)
    if ((1 << i) & m)
      m2 |= 3 << (i * 2);
  return m2;
}

void Gpio_imx_chip::multi_config_pad(Pin_slice const &mask,
                                     unsigned func, unsigned value)
{
  unsigned m = mask.mask << mask.offset;
  unsigned v = value << mask.offset;

  switch (func)
    {
    case GPIO_DR:
    case GPIO_GDIR:
    case GPIO_IMR:
    case GPIO_EDGE_SEL:
      _regs[func].modify(m, m & v);
      break;
    case GPIO_PSR:
      throw -L4_EINVAL;
    case GPIO_ICR1:
      if (mask.offset)
        throw -L4_EINVAL;

      if (unsigned m2 = bitmask_blow_16_to_32(m))
        _regs[func].modify(m2, m2 & v);
      break;
    case GPIO_ICR2:
      if (mask.offset)
        throw -L4_EINVAL;

      if (unsigned m2 = bitmask_blow_16_to_32(m >> 16))
        _regs[func].modify(m2, m2 & v);
      break;
    case GPIO_ISR: // w1c -- write 1 to clear
      _regs[func] = m & v;

      if (m & v & 0xffff || !_irq_svr_secondary)
        _irq_svr->enable();
      else if ((m & v) >> 16)
        _irq_svr_secondary->enable();
      break;
    default:
      throw -L4_EINVAL;
    }
}

void
Gpio_imx_chip::config_get(unsigned pin, unsigned func, unsigned *value)
{
  switch (func)
    {
    case GPIO_DR:
    case GPIO_GDIR:
    case GPIO_PSR:
      *value = (_regs[func] >> pin) & 1;
      break;
    case GPIO_ICR1:
    case GPIO_ICR2:
      if (pin > 15)
        throw -L4_EINVAL;
      *value = _regs[func] & (3 << (pin * 2));
      break;
    case GPIO_IMR:
    case GPIO_ISR:
    case GPIO_EDGE_SEL:
      *value = (_regs[func] >> pin) & 1;
      break;
    default:
      throw -L4_EINVAL;
    }
}

void Gpio_imx_chip::multi_setup(Pin_slice const &mask,
                                unsigned mode, unsigned outvalues)
{
  d_printf(DBG_ERR,
           "Not implemented: multi_setup(mask={0x%x,0x%08x}, mode=0x%x, outvalues=0x%x)\n",
           mask.offset, mask.mask, mode, outvalues);
}

void
Gpio_imx_chip::config_pull(unsigned, unsigned)
{
  // No such possibility with the GPIO
  throw -L4_EINVAL;
}

Io_irq_pin *
Gpio_imx_chip::get_irq(unsigned pin)
{
  if (pin >= nr_pins())
    throw -L4_EINVAL;

  Io_irq_pin *i = _irq_svr->get_pin<Irq_pin>(pin, _regs);
  if (i)
    i->set_mode(L4_IRQ_F_LEVEL_HIGH);
  return i;
}

void Gpio_imx_chip::init()
{
  Gpio_device::init();

  d_printf(DBG_INFO, "%s: init() %p\n", name(), this);

  Resource *regs = resources()->find("regs");
  // TODO: also look for reg0 if 'regs' not found -- or look for some
  // Mmio_res independent of name
  if (!regs || regs->type() != Resource::Mmio_res)
    {
      d_printf(DBG_ERR, "error: %s: no base address set\n"
                        "       missing or wrong 'regs' resource\n", name());
      throw "gpio-imx init error";
    }

  l4_addr_t phys_base = regs->start();
  l4_addr_t size = regs->size();

  if (size < 0x20 || size > 0x4000)
    {
      d_printf(DBG_ERR, "error: %s: invalid mmio size (%lx).\n", name(), size);
      throw "gpio-imx init error";
    }

  l4_addr_t vbase = res_map_iomem(phys_base, size);
  if (!vbase)
    {
      d_printf(DBG_ERR, "error: %s: cannot map registers: phys=%lx-%lx",
               name(), phys_base, phys_base + size - 1);
      throw "gpio-imx init error";
    }

  d_printf(DBG_ERR, "%s: mapped %lx registers to %08lx\n",
           name(), phys_base, vbase);

  _regs = new L4drivers::Mmio_register_block<32>(vbase);

  _regs[GPIO_IMR] = 0;
  _regs[GPIO_ISR] = ~0;

  Resource *irq = resources()->find("irq0");
  // TODO: look for two IRQs independent of some name
  if (!irq || irq->type() != Resource::Irq_res)
    d_printf(DBG_WARN, "warning: %s: no 'irq0' configured\n"
                       "         no IRQs available for pins 0-15\n", name());

  _irq_svr = new Irq_server(irq->start(), irq->flags(), _regs);

  irq = resources()->find("irq1");
  if (!irq || irq->type() != Resource::Irq_res)
    d_printf(DBG_WARN, "warning: %s: no 'irq1' configured\n"
                       "         no IRQs available for pins 16-31\n", name());
  else
    _irq_svr_secondary = new Irq_server_secondary(irq->start(), irq->flags(),
                                                _irq_svr);

  // TODO use irq-type from resource in get_irq

  d_printf(DBG_INFO, "gpio-imx driver ready\n");
}

class Gpio_imx8qm_chip : public Hw::Scu_device<Gpio_imx_chip>
{};

static Hw::Device_factory_t<Gpio_imx_chip> __hw_pf_factory("Gpio_imx_chip");
static Hw::Device_factory_t<Gpio_imx8qm_chip> __hw_pf_factory1("Gpio_imx8qm_chip");
}
