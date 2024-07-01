PKGDIR		?= ../../../..
L4DIR		?= $(PKGDIR)/../../..

SUBDIRS :=
SRC_CC-$(CONFIG_L4IO_GPIO_BCM2835) += bcm2835.cc
SRC_CC-$(CONFIG_L4IO_GPIO_OMAP)    += omap.cc
SRC_CC-$(CONFIG_L4IO_GPIO_QCOM)    += qcom.cc
SRC_CC-$(CONFIG_L4IO_GPIO_IMX)     += imx.cc

include $(PKGDIR)/server/src/lib_subdir.mk
