JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript

$(KBUILD_ZEPHYR_APP):
	@echo "Building" $@
	$(MAKE) -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr BOARD=${BOARD} VARIETY=${VARIETY} jerry
	cp $(JERRY_BASE)/build/${BOARD}/$@ $(O)
