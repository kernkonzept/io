// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

/**
 * Driver for the PCIe controller of RCar-3 boards.
 *
 * The Linux device tree for such devices looks like this:
 *
 * \code{.dts}
 * pciec0: pcie@fe000000 {
 *         compatible = "renesas,pcie-r8a7795",
 *                      "renesas,pcie-rcar-gen3";
 *         reg = <0 0xfe000000 0 0x80000>;
 *         #address-cells = <3>;
 *         #size-cells = <2>;
 *         bus-range = <0x00 0xff>;
 *         device_type = "pci";
 *         ranges = <0x01000000 0 0x00000000 0 0xfe100000 0 0x00100000
 *                   0x02000000 0 0xfe200000 0 0xfe200000 0 0x00200000
 *                   0x02000000 0 0x30000000 0 0x30000000 0 0x08000000
 *                   0x42000000 0 0x38000000 0 0x38000000 0 0x08000000>;
 *         dma-ranges = <0x42000000 0 0x40000000 0 0x40000000 0 0x40000000>;
 *         interrupts = <GIC_SPI 116 IRQ_TYPE_LEVEL_HIGH>,
 *                      <GIC_SPI 117 IRQ_TYPE_LEVEL_HIGH>,
 *                      <GIC_SPI 118 IRQ_TYPE_LEVEL_HIGH>;
 *         #interrupt-cells = <1>;
 *         interrupt-map-mask = <0 0 0 0>;
 *         interrupt-map = <0 0 0 0 &gic GIC_SPI 116 IRQ_TYPE_LEVEL_HIGH>;
 *         ...
 * };
 * \endcode
 *
 * This entry should be converted into the following io.cfg code:
 *
 * \code{.lua}
 * pciec0 = Io.Hw.Rcar3_pcie_bridge(function()
 *   Property.regs_base  = 0xfe000000; -- controller registers, see 'reg'
 *   Property.regs_size  = 0x00080000;
 *   Property.mem_base_1 = 0xfe100000; -- 1st window, see 'ranges'
 *   Property.mem_size_1 = 0x00100000;
 *   Property.mem_base_2 = 0xfe200000; -- 2nd window, see 'ranges'
 *   Property.mem_size_2 = 0x00200000;
 *   Property.mem_base_3 = 0x30000000; -- 3rd window, see 'ranges'
 *   Property.mem_size_3 = 0x08000000;
 *   Property.mem_base_4 = 0x38000000; -- 4th window, see 'ranges'
 *   Property.mem_size_4 = 0x08000000;
 *   Property.irq_1      = 32 + 116;   -- 1st interrupt, see 'interrupts'
 *   Property.irq_2      = 32 + 117;   -- 2nd interrupt, see 'interrupts'
 *   Property.irq_pme    = 32 + 118;   -- 3rd interrupt, see 'interrupts'
 * end)
 * \endcode
 *
 */

#include "cpg_rcar3.h"
#include "hw_device.h"
#include "hw_mmio_register_block.h"
#include "pci.h"
#include "resource_provider.h"

#include <l4/re/error_helper>
#include <l4/util/util.h>

enum
{
  // More investigation required.
  Enable_msi = 0
};

namespace {

class Rcar3_pcie_bridge
: public Hw::Device,
  public Hw::Pci::Root_bridge
{
public:
  Rcar3_pcie_bridge(int segment = 0, unsigned bus_nr = 0)
  : Hw::Device(),
    Hw::Pci::Root_bridge(segment, bus_nr, Pci_express_bus, this)
  {
    // the set of mandatory properties
    register_property("regs_base", &_regs_base);
    register_property("regs_size", &_regs_size);
    register_property("mem_base_1", &_mem_base_1);
    register_property("mem_size_1", &_mem_size_1);
    register_property("mem_base_2", &_mem_base_2);
    register_property("mem_size_2", &_mem_size_2);
    register_property("mem_base_3", &_mem_base_3);
    register_property("mem_size_3", &_mem_size_3);
    register_property("mem_base_4", &_mem_base_4);
    register_property("mem_size_4", &_mem_size_4);
    register_property("irq_1", &_int_map[0]);
    register_property("irq_2", &_int_map[1]);
    register_property("irq_pme", &_int_pme);

    set_name("Rcar3 PCIe root bridge");
  }

  typedef Hw::Pci::Cfg_addr Cfg_addr;
  typedef Hw::Pci::Cfg_width Cfg_width;

  void init();

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) override;
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) override;

  int int_map(int i) const { return _int_map[i]; }

private:
  enum
  {
    Pcie_car     = 0x0010,              // configuration transmission address
    Pcie_cctlr   = 0x0018,              // configuration transmission control
    Pcie_cctlr_ccie     = 1U << 31,
    Pcie_cctlr_type     = 1 << 8,
    Pcie_cdr     = 0x0020,              // configuration transmission data
    Pcie_msr     = 0x0028,              // mode setting
    Pcie_msr_endpoint   = 0 << 0,
    Pcie_msr_rootport   = 1 << 0,
    Pcie_intxr   = 0x0400,              // INTx register
    Pcie_physr   = 0x07f0,              // physical layer status
    Pcie_msitxr  = 0x0840,              // MSI transmission
    Pcie_msitxr_msie = 1U << 31,
    Pcie_msitxr_mmenum_shft = 16,
    Pcie_tctlr   = 0x2000,              // transfer control register
    Pcie_tctlr_initstrt = 0 << 0,       //   start initialization
    Pcie_tctlr_initdone = 1 << 0,       //   configuration initialization done
    Pcie_tstr    = 0x2004,              // transfer status register
    Pcie_tstr_dllact    = 1 << 0,
    Pcie_intr    = 0x2008,              // interrupt register
    Pcie_errfr   = 0x2020,              // error factor register
    Pcie_errfr_rcvurcpl = 1 << 4,
    Pcie_msialr  = 0x2048,              // MSI address lower register
    Pcie_msiaur  = 0x204c,              // MSI address upper register
    Pcie_msiier  = 0x2050,              // MSI interrupt enable register
    Pcie_prar0   = 0x2080,              // root port address register 0
    Pcie_prar1   = 0x2084,              // root port address register 1
    Pcie_prar2   = 0x2088,              // root port address register 2
    Pcie_prar3   = 0x208c,              // root port address register 3
    Pcie_prar4   = 0x2090,              // root port address register 4
    Pcie_prar5   = 0x2094,              // root port address register 5
    Pcie_lar0    = 0x2200,              // local address register 0
    Pcie_lamr0   = 0x2208,              // local address mask register 0
    Pcie_lamr_16        = 0 << 4,
    Pcie_lamr_32        = ((1 << 1) - 1) << 4,
    Pcie_lamr_64        = ((1 << 2) - 1) << 4,
    Pcie_lamr_128       = ((1 << 3) - 1) << 4,
    Pcie_lamr_256       = ((1 << 4) - 1) << 4,
    Pcie_lamr_512       = ((1 << 5) - 1) << 4,
    Pcie_lamr_256mb     = ((1 << 24) - 1) << 4,
    Pcie_lamr_512mb     = ((1 << 25) - 1) << 4,
    Pcie_lamr_1gb       = ((1 << 26) - 1) << 4,
    Pcie_lamr_2gb       = ((1 << 27) - 1) << 4,
    Pcie_lamr_4gb       = ((1 << 28) - 1) << 4,
    Pcie_lamr_io        = 1 << 0,
    Pcie_lamr_mmio      = 0 << 0,
    Pcie_lamr_laren     = 1 << 1,
    Pcie_lamr_mmio32bit = 0 << 2,
    Pcie_lamr_mmio64bit = 1 << 2,
    Pcie_lamr_mmiopref  = 1 << 3,
    Pcie_lamr_mmionpref = 0 << 3,
    Pcie_lamr_io16b     = 1 << 3,
    Pcie_lamr_io8b      = 1 << 2,
    Pcie_lamr_io4b      = 0 << 2,
    Pcie_lar1    = 0x2220,              // local address register 1
    Pcie_lamr1   = 0x2228,              // local address mask register 1
    Pcie_lar2    = 0x2240,              // local address register 2
    Pcie_lamr2   = 0x2248,              // local address mask register 2
    Pcie_lar3    = 0x2260,              // local address register 3
    Pcie_lamr3   = 0x2268,              // local address mask register 3

    Pcie_palr0   = 0x3400,              // PCIEC address lower register 0
    Pcie_paur0   = 0x3404,              // PCIEC address upper register 0
    Pcie_pamr0   = 0x3408,              // PCIEC address mask register 0
    Pcie_ptctlr0 = 0x340c,              // PCIEC conversion control register 0
    Pcie_ptctlr_pare    = 1 << 31,
    Pcie_ptctlr_spcio   = 1 << 8,
    Pcie_ptctlr_spcmmio = 0 << 8,
    Pcie_palr1   = 0x3420,              // PCIEC address lower register 1
    Pcie_paur1   = 0x3424,              // PCIEC address upper register 1
    Pcie_pamr1   = 0x3428,              // PCIEC address mask register 1
    Pcie_ptctlr1 = 0x342c,              // PCIEC conversion control register 1
    Pcie_palr2   = 0x3440,              // PCIEC address lower register 2
    Pcie_paur2   = 0x3444,              // PCIEC address upper register 2
    Pcie_pamr2   = 0x3448,              // PCIEC address mask register 2
    Pcie_ptctlr2 = 0x344c,              // PCIEC conversion control register 2
    Pcie_palr3   = 0x3460,              // PCIEC address lower register 3
    Pcie_paur3   = 0x3464,              // PCIEC address upper register 3
    Pcie_pamr3   = 0x3468,              // PCIEC address mask register 3
    Pcie_ptctlr3 = 0x346c,              // PCIEC conversion control register 3

    // PCI configuration registers (of the root bridge). See also PCI spec
    //  "Configuration Space" but the RCar3 boards add more registers / bits.
    Pciconf0     = 0x10000,             // vendor ID + device ID
    Pciconf1     = 0x10004,             // command + status
    Pciconf1_parerr     = 1 << 31,      //  detected parity error
    Pciconf1_err        = 1 << 30,      //  signaled system error
    Pciconf1_rma        = 1 << 29,      //  received master abort
    Pciconf1_rta        = 1 << 28,      //  received target abort
    Pciconf1_sta        = 1 << 27,      //  signaled target abort
    Pciconf1_mdpe       = 1 << 24,      //  master data parity error
    Pciconf1_fbtc       = 1 << 23,      //  fast back-to-back transaction cpbl
    Pciconf2     = 0x10008,             // revision ID + class code
    Pciconf3     = 0x1000c,             // cache line size, latency timer etc
    Pciconf4     = 0x10010,             // base address register 0
    Pciconf5     = 0x10014,             // base address register 1
    Pciconf6     = 0x10018,             // primary/secondary/subordinate busnr
    Pciconf7     = 0x1001c,             // IO base/limit + secondary status
    Pciconf8     = 0x10020,             // memory base / limit
    Pciconf9     = 0x10024,             // prefetchable mem base / limit
    Pciconf10    = 0x10028,             // prefetchable mem base upper 32-bit
    Pciconf11    = 0x1002c,             // prefetchable mem limit upper 32-bit
    Pciconf12    = 0x10030,             // IO base/limit upper 16-bit
    Pciconf13    = 0x10034,             // capabilities pointer
    Pciconf14    = 0x10038,             // expansion ROM
    Pciconf15    = 0x1003c,             // int line + int pin + bridge control

    // PCI power management
    Pmcap0       = 0x10040,
    Pmcap1       = 0x10044,

    // MSI capability registers
    Msicap0      = 0x10050,
    Msicap1      = 0x10054,
    Msicap2      = 0x10058,
    Msicap3      = 0x1005c,
    Msicap4      = 0x10060,
    Msicap5      = 0x10064,

    // PCIe capability registers
    Expcap0      = 0x10070,
    Expcap1      = 0x10074,
    Expcap2      = 0x10078,
    Expcap3      = 0x1007c,
    Expcap4      = 0x10080,
    Expcap5      = 0x10084,
    Expcap6      = 0x10088,
    Expcap7      = 0x1008c,
    Expcap8      = 0x10090,
    Expcap9      = 0x10094,
    Expcap10     = 0x10098,
    Expcap11     = 0x1009c,
    Expcap12     = 0x100a0,
    Expcap13     = 0x100a4,
    Expcap14     = 0x100a8,
    // VC capability registers
    Vccap0       = 0x10100,
    Vccap1       = 0x10104,
    Vccap2       = 0x10108,
    Vccap3       = 0x1010c,
    Vccap4       = 0x10110,
    Vccap5       = 0x10114,
    Vccap6       = 0x10118,
    // PCIEC link layer control registers
    Idsetr0      = 0x11000,             // ID setting register 0
    Idsetr1      = 0x11004,             // ID setting register 1
    Tlctlr       = 0x11048,             // TL control register
  };

  int assert_prop(Int_property &prop, char const *prop_name);
  int host_init();
  int access_enable(Cfg_addr addr, Cfg_width width);
  void access_disable(Cfg_addr addr);
  void alloc_msi_page(void **virt, l4_addr_t *phys);
  void init_msi();

  Int_property _regs_base{~0};
  Int_property _regs_size{~0};
  Int_property _mem_base_1{~0};
  Int_property _mem_size_1{~0};
  Int_property _mem_base_2{~0};
  Int_property _mem_size_2{~0};
  Int_property _mem_base_3{~0};
  Int_property _mem_size_3{~0};
  Int_property _mem_base_4{~0};
  Int_property _mem_size_4{~0};
  Int_property _int_map[2];
  Int_property _int_pme{~0};

  // PCI root bridge core memory.
  Hw::Register_block<32> _regs;

  // Used for certain debug output.
  char _prefix[20];

  L4Re::Util::Unique_cap<L4Re::Dataspace> _ds_msi;
};

// return upper 32-bit part of a 64-bit value
static inline unsigned u64_hi(l4_uint64_t u64)
{ return u64 >> 32; }

// return lower 32-bit part of a 64-bit value
static inline unsigned u64_lo(l4_uint64_t u64)
{ return u64 & 0xffffffff; }

class Irq_router_rs : public Resource_space
{
public:
  bool request(Resource *parent, ::Device *pdev,
               Resource *child, ::Device *cdev) override
  {
    auto *cd = dynamic_cast<Hw::Device *>(cdev);
    if (!cd)
      return false;

    unsigned pin = child->start();
    if (pin > 1)
      return false;

    auto *pd = dynamic_cast<Rcar3_pcie_bridge *>(pdev);
    if (!pd)
      return false;

    int irq_nr = pd->int_map(pin);
    if (irq_nr < 0)
      return false;

    d_printf(DBG_ERR, "%s/%08x Requesting IRQ%c => IRQ %d\n",
             cd->get_full_path().c_str(), cd->adr(),
             (int)('A' + pin), irq_nr);

    child->del_flags(Resource::F_relative);
    child->start(irq_nr);
    child->del_flags(Resource::Irq_type_mask);
    child->add_flags(Resource::Irq_type_base | Resource::Irq_type_level_high);

    child->parent(parent);

    return true;
  }

  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to Irq_router_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Irq_router_rs\n");
    return false;
  }
};

int
Rcar3_pcie_bridge::assert_prop(Int_property &prop, char const *prop_name)
{
  if (prop == ~0)
    {
      d_printf(DBG_ERR, "ERROR: %s: '%s' not set.\n", _prefix, prop_name);
      return -L4_EINVAL;
    }

  return 0;
}

int
Rcar3_pcie_bridge::host_init()
{
  if (   assert_prop(_regs_base,  "regs_base")
      || assert_prop(_regs_size,  "regs_size")
      || assert_prop(_mem_base_1, "mem_base_1")
      || assert_prop(_mem_size_1, "mem_size_1")
      || assert_prop(_mem_base_2, "mem_base_2")
      || assert_prop(_mem_size_2, "mem_size_2")
      || assert_prop(_mem_base_3, "mem_base_3")
      || assert_prop(_mem_size_3, "mem_size_3")
      || assert_prop(_mem_base_4, "mem_base_4")
      || assert_prop(_mem_size_4, "mem_size_4")
      || assert_prop(_int_map[0], "irq_1")
      || assert_prop(_int_map[1], "irq_2")
      || assert_prop(_int_pme,    "irq_pme"))
    return -L4_EINVAL;

  l4_addr_t va = res_map_iomem(_regs_base, _regs_size);
  if (!va)
    {
      d_printf(DBG_ERR, "ERROR: %s: could not map core memory.\n", _prefix);
      return -L4_ENOMEM;
    }
  _regs = new Hw::Mmio_register_block<32>(va);

  unsigned bit;
  Rcar3_cpg cpg(0xe6150000);
  if (_regs_base == 0xfe000000)         // pcie0
    bit = 19;
  else if (_regs_base == 0xee800000)    // pcie1
    bit = 18;
  else
    {
      d_printf(DBG_ERR, "ERROR: unknown PCIe controller at %08llx -- fix CPG code!\n",
               _regs_base.val());
      return -L4_EINVAL;
    }

  int ret = cpg.enable_clock(3, bit);
  if (ret != L4_EOK)
    {
      d_printf(DBG_ERR,
               "ERROR: %s: couldn't enable PCIe controller at CPG (%s)!\n",
               _prefix, l4sys_errtostr(ret));
      return ret;
    }

  // set PCIe inbound ranges
  _regs[Pcie_prar0] = 0x40000000;
  _regs[Pcie_lar0]  = 0x40000000;
  _regs[Pcie_lamr0] = Pcie_lamr_1gb             // local address mask 1GiB
                    | Pcie_lamr_mmio            // MMIO
                    | Pcie_lamr_laren           // local address enable
                    | Pcie_lamr_mmio64bit       // 64-bit addresses used
                    | Pcie_lamr_mmiopref;       // prefetchable MMIO

  // clear because of Pcie_lamr0.Pcie_lamr_mmio64bit=1
  _regs[Pcie_prar1] = 0x00000000;
  _regs[Pcie_lar1]  = 0x00000000;
  _regs[Pcie_lamr1] = 0x00000000;

  // ====== BEGIN initialization ======
  _regs[Pcie_tctlr] = Pcie_tctlr_initstrt;

  // PCIe mode: Type 01 (root port)
  _regs[Pcie_msr] = Pcie_msr_rootport;

  // wait for PHY ready
  for (unsigned i = 0; i < 20 && !(_regs[Pcie_physr] & 1); ++i)
    l4_sleep(10);

  if (!(_regs[Pcie_physr] & 1))
    {
      d_printf(DBG_ERR, "ERROR: %s: PHY not ready!\n", _prefix);
      return -L4_ENXIO;
    }

  // ID setting register 1 (value reflected to Pciconf2)
  _regs[Idsetr1] = 0x6040000;

  // type01: secondary bus no: 1
  _regs[Pciconf6].modify(0x0000ff00, 0x00000100);

  // type01: subordinate bus no: 1
  _regs[Pciconf6].modify(0x00ff0000, 0x00010000);

  // PCIe capability list ID (0x10 fix)
  _regs[Expcap0].modify(0x000000ff, Hw::Pci::Cap::Pcie);

  // device port type: 4=root port of PCIe root complex
  _regs[Expcap0].modify(0x00f00000, 0x00400000);

  // root port => type01 layout
  _regs[Pciconf3].modify(0x007f0000, 0x00010000);

  // bit 20: Data link layer active reporting capable
  _regs[Expcap3].modify(0x00100000, 0x00100000);

  // physical slot number: 0 (actually "invalid" with RCar3)
  _regs[Expcap5].modify(0xfff80000, 0x00000000);

  // completion timer: 50ms
  _regs[Tlctlr].modify(0x00003f00, 0x00003200);

  // terminate cap list
  _regs[Vccap0].modify(0xfff00000, 0x00000000);

  if (Enable_msi)
    {
      // MSI: enable
      _regs[Pcie_msitxr] = Pcie_msitxr_msie
                         | (0x1f << Pcie_msitxr_mmenum_shft);
    }

  // IO / mmio /prefetchable memory disabled
  _regs[Pciconf7] = 0x000000f0; // no IO (base > limit)
  _regs[Pciconf8] = 0x0000fff0; // no memory (base > limit)
  _regs[Pciconf9] = 0x0000fff0; // no prefetchable memory (base > limit)

  // Finish initialization -- establish PCIe link
  // At this point the hardware sets the BARn registers based on PcielamrX.
  _regs[Pcie_tctlr] = Pcie_tctlr_initdone;
  // ====== DONE initialization ======

  // setup resource window 0
  _regs[Pcie_ptctlr0] = 0;
  _regs[Pcie_pamr0]   = (_mem_size_1 - 1) & ~0x7f;
  _regs[Pcie_paur0]   = u64_hi(_mem_base_1);
  _regs[Pcie_palr0]   = u64_lo(_mem_base_1);
  _regs[Pcie_ptctlr0] = Pcie_ptctlr_pare | Pcie_ptctlr_spcio;

  // setup resource window 1
  _regs[Pcie_ptctlr1] = 0;
  _regs[Pcie_pamr1]   = (_mem_size_2 - 1) & ~0x7f;
  _regs[Pcie_paur1]   = u64_hi(_mem_base_2);
  _regs[Pcie_palr1]   = u64_lo(_mem_base_2);
  _regs[Pcie_ptctlr1] = Pcie_ptctlr_pare | Pcie_ptctlr_spcmmio;

  // setup resource window 2
  _regs[Pcie_ptctlr2] = 0;
  _regs[Pcie_pamr2]   = (_mem_size_3 - 1) & ~0x7f;
  _regs[Pcie_paur2]   = u64_hi(_mem_base_3);
  _regs[Pcie_palr2]   = u64_lo(_mem_base_3);
  _regs[Pcie_ptctlr2] = Pcie_ptctlr_pare | Pcie_ptctlr_spcmmio;

  // setup resource window 3
  _regs[Pcie_ptctlr3] = 0;
  _regs[Pcie_pamr3]   = (_mem_size_4 - 1) & ~0x7f;
  _regs[Pcie_paur3]   = u64_hi(_mem_base_4);
  _regs[Pcie_palr3]   = u64_lo(_mem_base_4);
  _regs[Pcie_ptctlr3] = Pcie_ptctlr_pare | Pcie_ptctlr_spcmmio;

  // MSI??
  //  _regs[Pciconf8] = 0x30403000;
  //  _regs[Pciconf9] = 0x0000fff0;

  // wait until PCIe link is up
  for (unsigned i = 0; i < 20; ++i)
    {
      if (_regs[Pcie_tstr] & Pcie_tstr_dllact)
        {
          d_printf(DBG_INFO, "%s: link up.\n", _prefix);

          // Remember, we are in root port mode:

          // INTA enable (apparently INTB/INTC/INTD are not used)
          _regs[Pcie_intxr].modify(0x0000ff00, 0x00000100);

          // bit 15..8: interrupt pin but cannot be written to anything != 0
          // bit  7..0: set interrupt line to 0x00: INTx interrupt reported to
          //            INTC(why INTC and not INTA?). The default value is 0xff
          //             which means that no interrupt is reported.
          _regs[Pciconf15].modify(0x000000ff, 0x00000000);

          return L4_EOK;
        }

      l4_sleep(10);
    }

  d_printf(DBG_INFO, "%s: link down.\n", _prefix);
  return -L4_ENXIO;
}

/**
 * Helper for cfg_read()/cfg_write().
 */
int
Rcar3_pcie_bridge::access_enable(Cfg_addr addr, Cfg_width width)
{
  unsigned reg = addr.reg() & ~3;

  // crossing 32-bit addresses!
  if (reg != ((addr.reg() + (1 << width) - 1) & ~3))
    return -EIO;

  // root device is the PCI bridge itself
  if (addr.bus() == 0)
    return 0;

  _regs[Pcie_errfr].modify(0, 0); // clear errors

  _regs[Pcie_car] = ((addr.bus() & 0xff) << 24)
                  | ((addr.dev() & 0x1f) << 19)
                  | ((addr.fn()  &    7) << 16)
                  | reg;

  if (addr.dev() != 0)
    // type-1 access (bus+dev+fn+reg)
    // Actually that makes only sense if a PCIe-to-PCIe bridge is plugged into
    // the PCIe slot.
    _regs[Pcie_cctlr] = Pcie_cctlr_ccie | Pcie_cctlr_type;
  else
    // type-0 access (only fn+reg)
    _regs[Pcie_cctlr] = Pcie_cctlr_ccie;

  // check errors for "unsupported request"
  if (_regs[Pcie_errfr] & Pcie_errfr_rcvurcpl)
    return -EIO;

  // check for "master/target abort"
  if (_regs[Pciconf1] & (Pciconf1_rma|Pciconf1_rta))
    return -EIO;

  return 0;
}

/**
 * Helper for cfg_read()/cfg_write().
 */
void
Rcar3_pcie_bridge::access_disable(Cfg_addr addr)
{
  if (addr.bus() != 0)
    _regs[Pcie_cctlr] = 0;
}

void
Rcar3_pcie_bridge::alloc_msi_page(void **virt, l4_addr_t *phys)
{
  _ds_msi = L4Re::Util::make_unique_cap<L4Re::Dataspace>();
  L4Re::chksys(L4Re::Env::env()->mem_alloc()
               ->alloc(L4_PAGESIZE, _ds_msi.get(),
                       L4Re::Mem_alloc::Continuous));

  auto d = L4Re::chkcap(L4Re::Util::make_unique_cap<L4Re::Dma_space>(),
                        "Allocate DMA dataspace cap");
  L4Re::chksys(L4Re::Env::env()->user_factory()->create(d.get()),
               "Create DMA dataspace");
  L4Re::chksys(d->associate(L4::Ipc::Cap<L4::Task>(),
                            L4Re::Dma_space::Space_attrib::Phys_space),
               "Associate DMA space for CPU physical");
  l4_size_t phys_size;
  L4Re::Dma_space::Dma_addr phys_ram = 0;
  L4Re::chksys(d->map(L4::Ipc::make_cap_rw(_ds_msi.get()), 0, &phys_size,
                      L4Re::Dma_space::Attributes::None,
                      L4Re::Dma_space::Bidirectional, &phys_ram),
               "Map dataspace to DMA space");
  if (phys_ram < L4_PAGESIZE)
    throw(L4::Out_of_memory("not really"));

  *virt = 0;
  L4Re::chksys(L4Re::Env::env()->rm()
               ->attach(virt, L4_PAGESIZE,
                        L4Re::Rm::F::Search_addr | L4Re::Rm::F::Eager_map
                        | L4Re::Rm::F::RW,
                        L4::Ipc::make_cap_rw(_ds_msi.get())),
               "Attach dataspace");

  *phys = phys_ram;

  d_printf(DBG_INFO, "%s: alloc_msi_page: virt=%08lx phys=%08lx\n",
           _prefix, (unsigned long)*virt, *phys);
}

void
Rcar3_pcie_bridge::init_msi()
{
  void *virt;
  l4_addr_t phys;
  alloc_msi_page(&virt, &phys);

  _regs[Pcie_msialr] = u64_lo(phys) | 1;
  _regs[Pcie_msiaur] = u64_hi(phys);
  _regs[Pcie_msiier] = 0xffffffff;
}

int
Rcar3_pcie_bridge::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width width)
{
  uint32_t v;

  if (access_enable(addr, width) < 0)
    v = 0xffffffff;
  else
    {
      if (addr.bus() == 0)
        {
          if (addr.dev() != 0)
            v = 0xffffffff;
          else
            v = _regs[Pciconf0 + (addr.reg() & ~3)];
        }
      else
        v = _regs[Pcie_cdr];
    }
  access_disable(addr);

  switch (width)
    {
    case Hw::Pci::Cfg_long:
      *value = v;
      break;
    case Hw::Pci::Cfg_short:
      *value = (v >> ((addr.reg() & 2) << 3)) & 0xffff;
      break;
    case Hw::Pci::Cfg_byte:
      *value = (v >> ((addr.reg() & 3) << 3)) & 0xff;
      break;
    default:
      return -EIO;
    }

  d_printf(DBG_ALL,
           "%s: cfg_read addr=%02x:%02x.%x reg=%03x width=%2d-bit value=%0*lx\n",
           _prefix, addr.bus(), addr.dev(), addr.fn(), addr.reg(), 8 << width,
           2 << width, *value & ((1UL << (8 << width)) - 1));

  return 0;
}

int
Rcar3_pcie_bridge::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width width)
{
  d_printf(DBG_ALL,
           "%s: cfg_wrte addr=%02x:%02x.%x reg=%03x width=%2d-bit value=%0*lx\n",
           _prefix, addr.bus(), addr.dev(), addr.fn(),  addr.reg(), 8 << width,
           2 << width, value & ((1UL << (8 << width)) - 1));

  if (access_enable(addr, width) < 0)
    return -EIO;

  if (addr.bus() == 0 && addr.dev() != 0)
    return 0;

  uint32_t mask, shift;
  switch (width)
    {
    case Hw::Pci::Cfg_long:
      shift = 0;
      mask  = 0xffffffff;
      break;
    case Hw::Pci::Cfg_short:
      shift = (addr.reg() & 2) << 3;
      mask  = 0xffff << shift;
      break;
    case Hw::Pci::Cfg_byte:
      shift = (addr.reg() & 3) << 3;
      mask  = 0xff << shift;
      break;
    default:
      return -EIO;
    }

  if (addr.bus() == 0)
    _regs[Pciconf0 + (addr.reg() & ~3)].modify(mask, (value << shift) & mask);
  else
    _regs[Pcie_cdr].modify(mask, (value << shift) & mask);

  access_disable(addr);

  return 0;
}

void
Rcar3_pcie_bridge::init()
{
  snprintf(_prefix, sizeof(_prefix), "rcar3_pcie.%08llx", _regs_base.val());

  if (host_init())
    return;

  d_printf(DBG_INFO, "%s: new device.\n", _prefix);

  if (Enable_msi)
    {
      try
        {
          init_msi();
        }
      // Errors during MSI initialization are fatal. This could change in the
      // future.
      catch (L4::Runtime_error &e)
        {
          if (e.extra_str() && e.extra_str()[0] != '\0')
            dprintf(DBG_ERR, "%s: %s: %s\n", _prefix, e.extra_str(), e.str());
          else
            dprintf(DBG_ERR, "%s: %s\n", _prefix, e.str());
          return;
        }
    }

  Resource *mr;

  // Ignore I/O ports in _mem_base_1 / _mem_size_1 as we don't support IO ports
  // on ARM anyway (at least for now).

  mr = new Resource_provider(Resource::Mmio_res);
  mr->start_size(_mem_base_2, _mem_size_2);
  mr->alignment(0xfffff);
  mr->set_id("MMIO");
  add_resource_rq(mr);

  mr = new Resource_provider(Resource::Mmio_res);
  mr->start_size(_mem_base_3, _mem_size_3);
  mr->alignment(0xfffff);
  mr->set_id("MMIO");
  add_resource_rq(mr);

  mr = new Resource_provider(Resource::Mmio_res | Resource::F_prefetchable);
  mr->start_size(_mem_base_4, _mem_size_4);
  mr->alignment(0xfffff);
  mr->set_id("MMIO");
  add_resource_rq(mr);

  auto *ir = new Hw::Pci::Irq_router_res<Irq_router_rs>();
  ir->set_id("IRQR");
  irq_router = ir;
  add_resource_rq(ir);

  discover_bus(this);

  Hw::Device::init();

  // Enable the bus master bit at the PCI host bridge.
  l4_uint32_t cmd;
  cfg_read(Cfg_addr(0, 0, 0, 0x04), &cmd, Hw::Pci::Cfg_short);
  cmd |= Hw::Pci::Dev::CC_bus_master;
  cfg_write(Cfg_addr(0, 0, 0, 0x04), cmd, Hw::Pci::Cfg_short);
}

static Hw::Device_factory_t<Rcar3_pcie_bridge> f("Rcar3_pcie_bridge");

}
