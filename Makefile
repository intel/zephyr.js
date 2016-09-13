BOARD ?= arduino_101_factory
KERNEL ?= micro
UPDATE ?= exit

# pass TRACE=y to trace malloc/free in the ZJS API
TRACE ?= n

ifndef ZJS_BASE
$(error ZJS_BASE not defined)
endif

JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript
JS ?= samples/HelloWorld.js
VARIANT ?= release

# Build for zephyr, default target
.PHONY: zephyr
zephyr: analyze generate
	@make -f Makefile.zephyr BOARD=$(BOARD) KERNEL=$(KERNEL)

.PHONY: analyze
analyze:
	@cat src/Makefile.base > src/Makefile
	@echo "ccflags-y += $(shell ./scripts/analyze.sh $(JS))" >> src/Makefile
	@if [ "$(TRACE)" = "y" ]; then \
		echo "ccflags-y += -DZJS_TRACE_MALLOC" >> src/Makefile; \
	fi

.PHONY: all
all: zephyr arc

# MAKECMDGOALS is a Make variable that is set to the target your building for.
# This is how we can check if we are building for linux and if a clean is needed.
# The linux target does not use the BOARD variable, so without this special
# case, the linux target would clean every time.
ifneq ($(MAKECMDGOALS), linux)
# Building for Zephyr, check for .$(BOARD).last_build to see if a clean is needed
ifeq ("$(wildcard .$(BOARD).last_build)", "")
PRE_ACTION=clean
endif
else
# Building for Linux, check for .linux.last_build to see if a clean is needed
ifeq ("$(wildcard .linux.last_build)", "")
PRE_ACTION=clean
endif
endif

# Update dependency repos using deps/repos.txt
.PHONY: update
update:
	@cd deps/; \
	while read line; do \
		case \
			"$$line" in \#*) \
				continue ;; \
			*) \
				name=$$(echo $$line | cut -d ' ' -f 1); \
				repo=$$(echo $$line | cut -d ' ' -f 2); \
				commit=$$(echo $$line | cut -d ' ' -f 3); \
				if [ ! -d $$name ]; then \
					git clone -q $$repo $$name; \
				fi; \
				echo Found dependency: $$name; \
				cd $$name; \
				if ! git checkout -q $$commit; then \
					if ! git pull -q; then \
						echo "Error attempting git pull on $$name"; \
					fi; \
					if ! git checkout -q $$commit; then \
						echo "Error checking out commit '$$commit' on $$name"; \
						if [ $(UPDATE) = "force" ]; then \
							echo "Continuing anyway (UPDATE=force)"; \
						else \
							echo "Exiting"; \
							exit 1; \
						fi; \
					fi; \
				fi; \
				cd ..; \
				continue ;; \
		esac \
	done < repos.txt
	@if ! env | grep -q ^ZEPHYR_BASE=; then \
		echo; \
		echo "ZEPHYR_BASE has not been set! It must be set to build"; \
		echo "e.g. export ZEPHYR_BASE=$(ZJS_BASE)/deps/zephyr"; \
		echo; \
		exit 1; \
	fi

# Sets up prj/last_build files
.PHONY: setup
setup: update
# Copy 256/216k board files to zephyr tree
	@rsync -a deps/overlay-zephyr/ deps/zephyr/
	@if [ $(BOARD) != qemu_x86 ]; then \
		cp prj.conf.arduino_101_factory prj.conf; \
	else \
		cp prj.conf.qemu_x86 prj.conf; \
	fi
# Remove .last_build file
	@rm -f .*.last_build
	@echo "" > .$(BOARD).last_build

# Explicit clean
.PHONY: clean
clean:
	@if [ -d deps/zephyr ] && [ -e src/Makefile ]; then \
		make -f Makefile.zephyr clean; \
	fi
	@if [ -d deps/jerryscript ]; then \
		make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr clean; \
		make -C $(JERRY_BASE) -f targets/zephyr/Makefile clean; \
		rm -rf deps/jerryscript/build/$(BOARD)/; \
	fi
	@rm -f src/*.o
	cd arc; make clean

# Flash Arduino 101 x86 image
.PHONY: dfu
dfu:
	dfu-util -a x86_app -D outdir/zephyr.bin

# Flash Arduino 101 ARC image
.PHONY: dfu-arc
dfu-arc:
	dfu-util -a sensor_core -D arc/outdir/zephyr.bin

# Flash both
.PHONY: dfu-all
dfu-all: dfu dfu-arc

# Generate the script file from the JS variable
.PHONY: generate
generate: setup $(PRE_ACTION)
	@echo Creating C string from JS application...
	@./scripts/convert.sh $(JS) src/zjs_script_gen.c

# Run QEMU target
.PHONY: qemu
qemu: analyze generate
	make -f Makefile.zephyr BOARD=qemu_x86 KERNEL=$(KERNEL) qemu

# Builds ARC binary
.PHONY: arc
arc:
ifeq ($(BOARD), arduino_101_factory_256)
	cd arc/; make BOARD=arduino_101_sss_factory_256
else
	cd arc/; make BOARD=arduino_101_sss_factory
endif

# Run debug server over JTAG
.PHONY: debug
debug:
	make -f Makefile.zephyr BOARD=arduino_101 debugserver

# Run gdb to connect to debug server for x86
.PHONY: gdb
gdb:
	gdb outdir/zephyr.elf -ex "target remote :3333"

# Run gdb to connect to debug server for ARC
.PHONY: arcgdb
arcgdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/i686-pokysdk-linux/usr/bin/arc-poky-elf/arc-poky-elf-gdb arc/outdir/zephyr.elf -ex "target remote :3334"

# Linux target
.PHONY: linux
# Linux command line target, script can be specified on the command line
linux: generate
	rm -f .*.last_build
	echo "" > .linux.last_build
	make -f Makefile.linux JS=$(JS) VARIANT=$(VARIANT)


.PHONY: help
help:
	@echo "Build targets:"
	@echo "    zephyr:    Build the main Zephyr target (default)"
	@echo "    arc:       Build the ARC Zephyr target for Arduino 101"
	@echo "    all:       Build the zephyr and arc targets"
	@echo "    linux:     Build the Linux target"
	@echo "    dfu:       Flash the x86 core binary with dfu-util"
	@echo "    dfu-arc:   Flash the ARC binary with dfu-util"
	@echo "    dfu-all:   Flash both binaries with dfu-util"
	@echo "    debug:     Run debug server using JTAG"
	@echo "    gdb:       Run gdb to connect to debug server for x86"
	@echo "    arcgdb:    Run gdb to connect to debug server for ARC"
	@echo "    qemu:      Run QEMU after building"
	@echo "    clean:     Clean stale build objects"
	@echo "    setup:     Sets up dependencies"
	@echo "    update:    Updates dependencies"
	@echo
	@echo "Build options:"
	@echo "    BOARD=     Specify a Zephyr board to build for"
	@echo "    JS=        Specify a JS script to compile into the binary"
	@echo "    KERNEL=    Specify the kernel to use (micro or nano)"
	@echo
