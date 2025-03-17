PKGDIR       ?= ../..
L4DIR        ?= $(PKGDIR)/../..

SRC_ASSETS_IO = $(patsubst $(SRC_DIR)/%,%,$(wildcard $(SRC_DIR)/plat-*/*.io)) \
                x86-legacy.devs

include $(L4DIR)/mk/assets.mk
