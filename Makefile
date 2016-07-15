BOARD ?= arduino_101_factory
KERNEL ?= micro

ifndef JERRY_BASE
$(error JERRY_BASE not defined)
endif

all: zephyr

# Find and set the last build target, so we know if a clean is needed
ifeq ($(shell grep arduino_101 outdir/include/generated/autoconf.h),)
LAST_BUILD=qemu_x86
else
LAST_BUILD=arduino_101_factory
endif

# Setup: Check the clean status, copy correct prj.conf.* file
ifneq ($(BOARD), $(LAST_BUILD))
setup: clean
else
setup:
endif
	cp prj.conf.$(BOARD) prj.conf

# Explicit clean
clean:
	make -f Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile clean
	rm -rf deps/jerryscript/build/$(BOARD)/

# Arduino 101 flash target
flash:
	dfu-util -a x86_app -D outdir/zephyr.bin

# Generate the script file from the JS variable
ifdef JS
generate: setup
	./scripts/convert.sh $(JS) src/zjs_script_gen.c
endif

# Run QEMU target
ifdef JS
qemu: generate
else
qemu: setup
endif
	make -f Makefile.zephyr BOARD=qemu_x86 KERNEL=$(KERNEL) qemu

# Build for zephyr, default target
ifdef JS
zephyr: generate
else
zephyr: setup
endif
	make -f Makefile.zephyr BOARD=$(BOARD) KERNEL=$(KERNEL)

help:
	@echo "Build targets:"
	@echo "    zephyr:    Build for zephyr"
	@echo "    flash:     Flash a zephyr binary"
	@echo "    qemu:      Run QEMU after building"
	@echo "    clean:     Clean stale build objects"
	@echo
	@echo "Build options:"
	@echo "    BOARD=     Specify a Zephyr board to build for"
	@echo "    JS=        Specify a JS script to compile into the binary"
	@echo "    KERNEL=    Specify the kernel to use (micro or nano)"
