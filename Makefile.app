JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript

$(KBUILD_ZEPHYR_APP):
	@echo "Building" $@
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr BOARD=$(BOARD) jerry
	cp $(JERRY_BASE)/build/$(BOARD)/obj-$(BOARD)/lib/$@ $(O)

mrproper:
	cd deps/zephyr; make mrproper
