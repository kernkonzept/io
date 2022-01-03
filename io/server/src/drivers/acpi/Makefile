PKGDIR		?= ../../../..
L4DIR		?= $(PKGDIR)/../../..

SUBDIRS :=
SRC_CC  := pci-root.cc \
           button.cc   \

SRC_CC-amd64	:= ec.cc
SRC_CC-x86	:= ec.cc
SRC_CC		+= $(SRC_CC-$(ARCH))

REQUIRES_LIBS = acpica

include $(PKGDIR)/server/src/lib_subdir.mk

WARNINGS += -Wno-unused-parameter
