BOARD ?= arduino_101_factory
KERNEL ?= micro

all: zephyr

setup:
	mkdir -p outdir/include
ifeq ($(BOARD), qemu_x86)
	cp prj.conf.x86 prj.conf
else
	cp prj.conf.arduino101 prj.conf
endif

clean:
	make -f Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr clean
	make -C $(JERRY_BASE) -f targets/zephyr/Makefile clean
	rm -rf deps/jerryscript/build/qemu_x86/

flash:
	dfu-util -a x86_app -D outdir/zephyr.bin

ifdef JS
generate: setup
	./scripts/convert.sh $(JS) src/zjs_script_gen.c
endif

ifdef JS
qemu: generate
else
qemu: setup
endif
	make -f Makefile.zephyr BOARD=qemu_x86 KERNEL=$(KERNEL) qemu

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
