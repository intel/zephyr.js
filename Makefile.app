JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript

EXT_JERRY_FLAGS ?= -DENABLE_ALL_IN_ONE=ON

ifneq ($(BOARD), frdm_k64f)
EXT_JERRY_FLAGS += -DENABLE_LTO=ON
endif

$(KBUILD_ZEPHYR_APP):
	@echo "Building" $@
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr BOARD=$(BOARD) EXT_JERRY_FLAGS="$(EXT_JERRY_FLAGS)" jerry
	cp $(JERRY_BASE)/build/$(BOARD)/obj-$(BOARD)/lib/$@ $(O)
