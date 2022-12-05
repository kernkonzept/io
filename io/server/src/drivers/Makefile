PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../../..

include $(PKGDIR)/server/src/Makefile.config

SUBDIRS                      = gpio
SUBDIRS-$(CONFIG_L4IO_ACPI) += acpi
SUBDIRS-$(CONFIG_L4IO_PCI)  += pci
SUBDIRS                     += $(SUBDIRS-y)

SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_ecam.cc
SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_rcar3.cc cpg_rcar3.cc
SRC_CC-$(CONFIG_L4IO_PCI)   += dwc_pcie_core.cc pcie_imx8.cc

SRC_CC += scu_imx8qm.cc

include $(PKGDIR)/server/src/lib_subdir.mk
