SOLETTA_INST_PATH := $(SOLETTA_BASE_DIR)/build/soletta_sysroot
SOLETTA_INCLUDE_PATH := $(SOLETTA_INST_PATH)/usr/include/soletta
SOLETTA_LIB_PATH := $(SOLETTA_INST_PATH)/usr/lib/libsoletta.a
SOLETTA_NODE_DESCRIPTIONS ?= $(SOLETTA_INST_PATH)/usr/share/soletta/flow/descriptions

# This is to force Soletta to be built before the application, so we ensure
# all the header files are somewhere the app will find them.
force_soletta_dep = $(SOLETTA_LIB_PATH)

scripts_basic: $(force_soletta_dep)

libs-y += lib/soletta/lib.a

SOLETTA_CFLAGS = $(KBUILD_CFLAGS) $(KBUILD_CPPFLAGS) $(ZEPHYRINCLUDE)
SOLETTA_LDFLAGS = $(LDFLAGS) $(LIB_INCLUDE_DIR)

# To keep the compiler from whining about .../sysgen not being there
SOLETTA_CFLAGS += -Wno-missing-include-dirs

export SOLETTA_CFLAGS SOLETTA_LDFLAGS

ZEPHYRINCLUDE += -I$(SOLETTA_INCLUDE_PATH)

$(SOLETTA_LIB_PATH):
	$(Q)$(MAKE) -f $(PWD)/Makefile.soletta
	$(Q)mkdir -p $(O)/lib/soletta
	$(Q)cp $(SOLETTA_LIB_PATH) $(O)/lib/soletta/lib.a
