ifneq ($(SYSTEM),)
# Io is C++11
SRC_CC_IS_CXX11    := c++0x
PRIVATE_INCDIR     += $(PKGDIR)/server/src

# do not generate PC files for this lib
PC_FILENAMES       :=
TARGET             := builtin.thin.a
NOTARGETSTOINSTALL := 1
SUBDIRS            += $(SUBDIRS_$(ARCH)) $(SUBDIRS_$(OSYSTEM))
SUBDIR_TARGETS     := $(addsuffix /OBJ-$(SYSTEM)/$(TARGET),$(SUBDIRS))
SUBDIR_OBJS         = $(addprefix $(OBJ_DIR)/,$(SUBDIR_TARGETS))
OBJS_$(TARGET)     += $(SUBDIR_OBJS)

# the all target must be first!
all::

# our bultin.a dependency
$(TARGET): $(SUBDIR_OBJS)

$(SUBDIR_OBJS): $(OBJ_DIR)/%/OBJ-$(SYSTEM)/$(TARGET): %
	$(VERBOSE)$(MAKE) $(MAKECMDGOALS) OBJ_BASE=$(OBJ_BASE) \
                          -C $(SRC_DIR)/$* $(MKFLAGS)
endif

include $(L4DIR)/mk/lib.mk
