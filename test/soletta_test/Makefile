check_vars := $(and $(SOLETTA_BASE_DIR),$(ZEPHYR_BASE),$(ZEPHYR_GCC_VARIANT),$(ZEPHYR_SDK_INSTALL_DIR))
vars_missing := $(if $(check_vars),"","yes")

define NL


endef

ifeq ("yes",$(vars_missing))
$(error $(NL)        Missing environment variables.$(NL)        The Zephyr build requires the following variables to be set:$(NL)$(NL)        ZEPHYR_BASE: Should point to the top Zephyr source directories.$(NL)        ZEPHYR_GCC_VARIANT: The toolchain to use to build Zephyr.$(NL)        ZEPHYR_SDK_INSTALL_DIR: The path where the Zephyr SDK is installed.$(NL)        SOLETTA_BASE_DIR: The path to source of Soletta.$(NL))
endif

KERNEL_TYPE ?= nano
BOARD ?= arduino_101
MDEF_FILE := prj.mdef

ifneq (,$(realpath $(PWD)/prj_$(BOARD).conf))
CONF_FILE := prj_$(BOARD).conf
else
ifneq (,$(realpath $(PWD)/prj.conf))
CONF_FILE := prj.conf
endif
endif

SOLETTA_CONF = $(realpath $(PWD)/sol_$(BOARD).conf)
ifeq (,$(SOLETTA_CONF))
SOLETTA_CONF = $(realpath $(PWD)/sol.conf)
endif
export SOLETTA_CONF

include $(ZEPHYR_BASE)/Makefile.inc

