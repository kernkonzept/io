PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../../..

SUBDIRS                      = gpio
SUBDIRS-$(CONFIG_L4IO_ACPI) += acpi
SUBDIRS-$(CONFIG_L4IO_PCI)  += pci
SUBDIRS                     += $(SUBDIRS-y)

SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_ecam.cc
SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_rcar3.cc cpg_rcar3.cc
SRC_CC-$(CONFIG_L4IO_PCI)   += dwc_pcie_core.cc pcie_imx8.cc

SRC_CC-$(CONFIG_L4IO_SCU_IMX8QM) += scu_imx8qm.cc

include $(PKGDIR)/server/src/lib_subdir.mk
