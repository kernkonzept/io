PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../../..

TARGET   := libio-vbus.a libio-vbus.so

SRC_CC   := vbus.cc vbus_pci.cc vbus_gpio.cc vbus_pm.cc
CXXFLAGS += -fno-rtti -fno-exceptions
PC_FILENAME := libio-vbus

include $(L4DIR)/mk/lib.mk
