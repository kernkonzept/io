/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <l4/sys/irq>
#include <l4/cxx/bitfield>
#include <l4/cxx/hlist>
#include <l4/re/util/cap_alloc>

class Io_irq_pin
{
public:
  enum Flags
  {
    F_shareable = 0x1,
  };
  typedef L4Re::Util::Ref_cap<L4::Triggerable>::Cap Triggerable;

private:
  int _sw_irqs;
  Triggerable _irq;
  unsigned short _flags;
  unsigned short _max_sw_irqs;

public:
  void chg_flags(bool set, unsigned flags)
  {
    if (set)
      _flags |= flags;
    else
      _flags &= ~flags;
  }

  Triggerable const &irq() const { return _irq; }

  Io_irq_pin() : _sw_irqs(0), _irq(), _flags(0), _max_sw_irqs(0) {}

  void set_shareable(bool s)
  { chg_flags(s, F_shareable); }

public:
  struct Msi_src
  {
    /**
     * Get MSI source-ID of device.
     *
     * The ID is used to get the required system Icu::msi_info().
     *
     * \param si[out]  The MSI source id for Icu::msi_info()
     *
     * \retval Negative error value or zero on success
     */
    virtual int get_msi_src_id(l4_uint64_t *si) = 0;
  };

  void add_sw_irq() { ++_max_sw_irqs; }
  int sw_irqs() const { return _sw_irqs; }
  void inc_sw_irqs() { ++_sw_irqs; }
  void dec_sw_irqs() { --_sw_irqs; }
  virtual int bind(Triggerable const &irq, unsigned mode) = 0;
  virtual int mask() = 0;
  virtual int unmask() = 0;
  virtual int unbind(bool deleted) = 0;
  virtual int set_mode(unsigned mode) = 0;
  virtual int clear();
  virtual int msi_info(Msi_src *, l4_icu_msi_info_t *)
  { return -L4_EINVAL; }
  virtual ~Io_irq_pin() = 0;

  bool shared() const { return _max_sw_irqs > 1; }
  bool shareable() const { return _flags & F_shareable; }
};

inline Io_irq_pin::~Io_irq_pin() {}
inline int Io_irq_pin::unbind(bool)
{
  _irq = L4::Cap_base::Invalid;
  return 0;
}

inline int Io_irq_pin::bind(Triggerable const &irq, unsigned)
{
  _irq = irq;
  return 0;
}

class Kernel_irq_pin : public Io_irq_pin
{
protected:
  unsigned _idx;

public:
  Kernel_irq_pin(unsigned idx) : Io_irq_pin(), _idx(idx) {}
  int bind(Triggerable const &irq, unsigned mode) override;
  int mask() override;
  int unmask() override;
  int unbind(bool deleted) override;
  int set_mode(unsigned mode) override;

  unsigned pin() const { return _idx; }

protected:
  int _msi_info(l4_uint64_t src, l4_icu_msi_info_t *);
};

class Msi_irq_pin
: public Kernel_irq_pin
{
public:
  Msi_irq_pin() : Kernel_irq_pin(0) {}
  ~Msi_irq_pin() noexcept;
  int unbind(bool deleted) override;
  int bind(Triggerable const &irq, unsigned mode) override;
  int msi_info(Msi_src *src, l4_icu_msi_info_t *) override;

private:
  void free_msi();
  int alloc_msi();
};
