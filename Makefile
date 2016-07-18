BOARD ?= arduino_101_factory
KERNEL ?= micro

ifndef JERRY_BASE
$(error JERRY_BASE not defined)
endif

.PHONY: all
all: zephyr arc

ifneq ("$(wildcard .$(BOARD).last_build)", "")
PRE_ACTION=build
else
PRE_ACTION=clean
endif

.PHONY: setup
# Setup: Check the clean status, copy correct prj.conf.* file
ifeq ($(PRE_ACTION), clean)
setup: clean
else
setup:
endif
	@if [ ! -d deps/jerryscript/ ]; then \
		git clone https://github.com/Samsung/jerryscript.git deps/jerryscript; \
		#cd deps/jerryscript/; git checkout master; \
		# Temp fix \
		cd deps/jerryscript/; git checkout a81c7c; \
	fi
	@if [ ! -d deps/zephyr/ ]; then \
		git clone https://gerrit.zephyrproject.org/r/zephyr deps/zephyr; \
		cd deps/zephyr/; git checkout master; \
	fi
	# Copy 256/216k board files to zephyr tree
	rsync -a deps/overlay-zephyr/ deps/zephyr/
	@if [ $(BOARD) != qemu_x86 ]; then \
		cp prj.conf.arduino_101_factory prj.conf; \
	else \
		cp prj.conf.qemu_x86 prj.conf; \
	fi
	# Remove .last_build file
	rm -f .*.last_build
	@echo "" > .$(BOARD).last_build

.PHONY: clean
# Explicit clean
clean:
	make -f Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile clean
	rm -rf deps/jerryscript/build/$(BOARD)/

.PHONY: flash
# Arduino 101 flash target
flash:
	dfu-util -a x86_app -D outdir/zephyr.bin

# Generate the script file from the JS variable
ifdef JS
.PHONY: generate
generate: setup
	./scripts/convert.sh $(JS) src/zjs_script_gen.c
endif

.PHONY: qemu
# Run QEMU target
ifdef JS
qemu: generate
else
qemu: setup
endif
	make -f Makefile.zephyr BOARD=qemu_x86 KERNEL=$(KERNEL) qemu

.PHONY: zephyr
# Build for zephyr, default target
ifdef JS
zephyr: generate
else
zephyr: setup
endif
	make -f Makefile.zephyr BOARD=$(BOARD) KERNEL=$(KERNEL)

.PHONY: arc
arc:
ifeq ($(BOARD), arduino_101_factory_256)
	cd arc/; make BOARD=arduino_101_sss_factory_256
else
	cd arc/; make BOARD=arduino_101_sss_factory
endif

.PHONY: help
help:
	@echo "Build targets:"
	@echo "    zephyr:    Build for zephyr (x86)"
	@echo "    flash:     Flash a zephyr binary"
	@echo "    qemu:      Run QEMU after building"
	@echo "    clean:     Clean stale build objects"
	@echo "    arc:       Build the ARC core target"
	@echo "    all:       Build the zephyr and arc targets"
	@echo
	@echo "Build options:"
	@echo "    BOARD=     Specify a Zephyr board to build for"
	@echo "    JS=        Specify a JS script to compile into the binary"
	@echo "    KERNEL=    Specify the kernel to use (micro or nano)"
