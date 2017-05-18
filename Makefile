# Copyright (c) 2016-2017, Intel Corporation.

# a place to add temporary defines to ZJS builds such as -DZJS_GPIO_MOCK
ZJS_FLAGS :=

OS := $(shell uname)

BOARD ?= arduino_101
RAM ?= 55
ROM ?= 144

# Dump memory information: on = print allocs, full = print allocs + dump pools
TRACE ?= off

ifneq (,$(DEV))
$(error DEV= is no longer supported, please use make ide or make ashell)
endif

ifndef ZJS_BASE
$(error ZJS_BASE not defined. You need to source zjs-env.sh)
endif

ifeq ($(BOARD), arduino_101)
# RAM can't be less than the 55KB normally allocated for x86
# NOTE: We could change this and allow it though, find a sane minimum
ifeq ($(shell test $(RAM) -lt 55; echo $$?), 0)
$(error RAM must be at least 55)
endif

# RAM can't be more than 79KB, the total size of RAM left for x86/arc
# NOTE: the first 1KB is reserved for ARC_READY bit
ifeq ($(shell test $(RAM) -gt 79; echo $$?), 0)
$(error RAM must be no higher than 79)
endif

# ROM can't be less than the 144KB physically allocated for x86
# NOTE: We could change this and allow it though, but splice images in reverse
ifeq ($(shell test $(ROM) -lt 144; echo $$?), 0)
$(error ROM must be at least 144)
endif

# ROM can't be more than 296KB, the total size of x86 + arc partitions combined
ifeq ($(shell test $(ROM) -gt 296; echo $$?), 0)
$(error ROM must be no higher than 296)
endif
endif  # BOARD = arduino_101

ifeq ($(filter $(MAKECMDGOALS),linux), linux)
$(error 'linux' make target is deprecated, use "make BOARD=linux")
endif

OCF_ROOT ?= deps/iotivity-constrained
JS ?= samples/HelloWorld.js
VARIANT ?= release
DEVICE_NAME ?= "ZJS Device"
BLE_ADDR ?= "none"
FUNC_NAME ?= off
# JerryScript options
JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript
EXT_JERRY_FLAGS ?=	-DENABLE_ALL_IN_ONE=ON \
			-DFEATURE_PROFILE=$(ZJS_BASE)/outdir/$(BOARD)/jerry_feature.profile \
			-DFEATURE_ERROR_MESSAGES=ON \
			-DJERRY_LIBM=OFF \
			-DJERRY_PORT_DEFAULT=OFF

# Generate and run snapshot as byte code instead of running JS directly
ifneq (,$(filter $(MAKECMDGOALS),ide ashell linux))
SNAPSHOT=off
# if the user passes in SNAPSHOT=on for ide, ashell, or linux give an error
ifeq ($(SNAPSHOT), on)
$(error ide, ashell, and linux do not support SNAPSHOT=$(SNAPSHOT))
endif
else
# snapshot is enabled by default
SNAPSHOT ?= on
ifeq ($(SNAPSHOT), on)
EXT_JERRY_FLAGS += -DFEATURE_JS_PARSER=OFF
endif
endif

ifeq ($(FUNC_NAME), on)
ZJS_FLAGS += " -DZJS_FIND_FUNC_NAME"
endif

# Settings for ashell builds
ifneq (,$(filter $(MAKECMDGOALS),ide ashell))
CONFIG ?= fragments/zjs.conf.dev
ASHELL=y
endif

ifeq ($(BOARD), arduino_101)
ifneq ($(OS), Darwin)
EXT_JERRY_FLAGS += -DENABLE_LTO=ON
endif
$(info makecmd: $(MAKECMDGOALS))
ifeq ($(MAKECMDGOALS),)
TARGETS=all
else
TARGETS=$(MAKECMDGOALS)
endif

# if actually building for A101 (not clean), report RAM/ROM allocation
ifneq ($(filter all zephyr arc,$(TARGETS)),)
$(info Building for Arduino 101...)
$(info $() $() RAM allocation: $(RAM)KB for X86, $(shell echo $$((79 - $(RAM))))KB for ARC)
$(info $() $() ROM allocation: $(ROM)KB for X86, $(shell echo $$((296 - $(ROM))))KB for ARC)
endif
endif  # BOARD = arduino_101

# Print callback statistics during runtime
CB_STATS ?= off
# Print floats (uses -u _printf_float flag). This is a workaround on the A101
# otherwise floats will not print correctly. It does use ~11k extra ROM though
PRINT_FLOAT ?= off

ifeq ($(BOARD), linux)
	SNAPSHOT = off
endif

ifeq ($(BOARD), arduino_101)
ARC = arc
endif

.PHONY: all
ifeq ($(BOARD), linux)
all: linux
else
all: zephyr
endif

.PHONY: check
check:
	trlite -l

.PHONY: quickcheck
quickcheck:
	trlite -l 3

A101BIN = outdir/arduino_101/zephyr.bin
A101SSBIN = arc/outdir/arduino_101_sss/zephyr.bin

.PHONY: ram_report
ram_report: zephyr
	@make -f Makefile.zephyr	BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					ZJS_FLAGS=$(ZJS_FLAGS) \
					ram_report

.PHONY: rom_report
rom_report: zephyr
	@make -f Makefile.zephyr	BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					ZJS_FLAGS=$(ZJS_FLAGS) \
					rom_report

# choose name of jerryscript library based on snapshot feature
ifeq ($(SNAPSHOT), on)
JERRYLIB=outdir/$(BOARD)/libjerry-core-snapshot.a
else
JERRYLIB=outdir/$(BOARD)/libjerry-core-parser.a
endif

.PHONY: flash
flash:  analyze generate $(JERRYLIB) $(ARC)
	@make -f Makefile.zephyr flash	BOARD=$(BOARD) \
		VARIANT=$(VARIANT) \
		CB_STATS=$(CB_STATS) \
		PRINT_FLOAT=$(PRINT_FLOAT) \
		SNAPSHOT=$(SNAPSHOT) \
		BLE_ADDR=$(BLE_ADDR) \
		ASHELL=$(ASHELL) \
		NETWORK_BUILD=$(NET_BUILD)

# Build for zephyr, default target
.PHONY: zephyr
zephyr: analyze generate $(JERRYLIB) $(ARC)
	@make -f Makefile.zephyr \
					BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					BLE_ADDR=$(BLE_ADDR) \
					ASHELL=$(ASHELL) \
					NETWORK_BUILD=$(NET_BUILD) \
					ZJS_FLAGS=$(ZJS_FLAGS)
ifeq ($(BOARD), arduino_101)
	@echo
	@echo -n Creating dfu images...
	@dd if=$(A101BIN) of=$(A101BIN).dfu bs=1024 count=144 2> /dev/null
	@dd if=$(A101BIN) of=$(A101SSBIN).dfu bs=1024 skip=144 2> /dev/null
	@dd if=$(A101SSBIN) of=$(A101SSBIN).dfu bs=1024 seek=$$(($(ROM) - 144)) 2> /dev/null
	@echo " done."
endif

.PHONY: ide
ide: zephyr

.PHONY: ashell
ashell: zephyr

# Flash Arduino 101 x86 and arc images
.PHONY: dfu
dfu:
	dfu-util -a x86_app -D $(A101BIN).dfu
	dfu-util -a sensor_core -D $(A101SSBIN).dfu

# Build JerryScript as a library (libjerry-core.a)
$(JERRYLIB):
	@echo "Building" $@
	@rm -rf $(JERRY_BASE)/build/$(BOARD)/
	$(MAKE) -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr BOARD=$(BOARD) EXT_JERRY_FLAGS="$(EXT_JERRY_FLAGS)" jerry
	mkdir -p outdir/$(BOARD)/
	cp $(JERRY_BASE)/build/$(BOARD)/obj-$(BOARD)/lib/libjerry-core.a $(JERRYLIB)

# Give an error if we're asked to create the JS file
$(JS):
	$(error The file $(JS) does not exist.)

# Find the modules the JS file depends on
.PHONY: analyze
analyze: $(JS)
	@echo "% This is a generated file" > prj.mdef
	@echo "# This is a generated file" > src/Makefile
	@cat src/Makefile.base >> src/Makefile
	@echo "# This is a generated file" > src/sensors/Makefile
	@cat src/sensors/Makefile.base >> src/sensors/Makefile
	@if [ "$(TRACE)" = "on" ] || [ "$(TRACE)" = "full" ]; then \
		echo "ccflags-y += -DZJS_TRACE_MALLOC" >> src/Makefile; \
	fi
	@if [ "$(SNAPSHOT)" = "on" ]; then \
		echo "ccflags-y += -DZJS_SNAPSHOT_BUILD" >> src/Makefile; \
	fi
	@echo "ccflags-y += $(shell ./scripts/analyze.sh $(BOARD) $(JS) $(CONFIG) $(ASHELL))" | tee -a src/Makefile src/sensors/Makefile
	@if [ "$(OS)" = "Darwin" ]; then \
		sed -i.bu '/This is a generated file/r./zjs.conf.tmp' src/Makefile; \
	else \
		sed -i '/This is a generated file/r./zjs.conf.tmp' src/Makefile; \
	fi
	@# Add bluetooth debug configs if BLE is enabled
	@if grep -q BUILD_MODULE_BLE src/Makefile; then \
		if [ "$(VARIANT)" = "debug" ]; then \
			echo "CONFIG_BLUETOOTH_DEBUG_LOG=y" >> prj.conf.tmp; \
		fi \
	fi
	@# Add the include for the OCF Makefile only if the script is using OCF
	@if grep -q BUILD_MODULE_OCF src/Makefile; then \
		echo "CONFIG_BLUETOOTH_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf.tmp; \
		echo "include \$$(ZJS_BASE)/Makefile.ocf_zephyr" >> src/Makefile; \
	fi
	@if grep -q BUILD_MODULE_DGRAM src/Makefile; then \
		echo "CONFIG_BLUETOOTH_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf.tmp; \
	fi
	@if [ -e outdir/$(BOARD)/jerry_feature.profile.bak ]; then \
		if ! cmp outdir/$(BOARD)/jerry_feature.profile.bak outdir/$(BOARD)/jerry_feature.profile; \
		then \
			rm -f outdir/$(BOARD)/libjerry-core*.a; \
		fi \
	else \
		rm -f outdir/$(BOARD)/libjerry-core*.a; \
	fi

# Update dependency repos
.PHONY: update
update:
	@git submodule update --init
	@cd $(OCF_ROOT); git submodule update --init;

# set up prj.conf file
-.PHONY: setup
setup:
	@echo "# This is a generated file" > prj.conf
	@cat fragments/prj.conf.base >> prj.conf

ifeq ($(BOARD), qemu_x86)
ifeq ($(OS), Darwin)
	@cat fragments/prj.conf.qemu_x86_osx >> prj.conf
else
	@cat fragments/prj.conf.qemu_x86 >> prj.conf
endif
else
ifeq ($(ASHELL), y)
	@cat fragments/prj.conf.ashell >> prj.conf
	@cat fragments/prj.mdef.ashell >> prj.mdef
ifeq ($(filter ide,$(MAKECMDGOALS)),ide)
	@echo CONFIG_USB_CDC_ACM=n >> prj.conf
else
	@echo CONFIG_USB_CDC_ACM=y >> prj.conf
endif
endif
ifeq ($(BOARD), arduino_101)
	@cat fragments/prj.conf.arduino_101 >> prj.conf
ifeq ($(OS), Darwin)
	# work around for OSX where the xtool toolchain do not
	# support iamcu instruction set on the Arduino 101
	@echo "CONFIG_X86_IAMCU=n" >> prj.conf
endif
	@printf "CONFIG_PHYS_RAM_ADDR=0xA800%x\n" $$(((80 - $(RAM)) * 1024)) >> prj.conf
	@echo "CONFIG_RAM_SIZE=$(RAM)" >> prj.conf
	@echo "CONFIG_ROM_SIZE=$(ROM)" >> prj.conf
	@printf "CONFIG_SS_RESET_VECTOR=0x400%x\n" $$((($(ROM) + 64) * 1024)) >> prj.conf
endif
ifeq ($(BOARD), frdm_k64f)
	@cat fragments/prj.conf.frdm_k64f >> prj.conf
endif
endif
# Append script specific modules to prj.conf
	@if [ -e prj.conf.tmp ]; then \
		cat prj.conf.tmp >> prj.conf; \
	fi

.PHONY: cleanlocal
cleanlocal:
	@rm -f src/*.o
	@rm -f src/Makefile
	@rm -f arc/prj.conf
	@rm -f arc/prj.conf.tmp
	@rm -f arc/src/Makefile
	@rm -f arc/outdir/arduino_101_sss/zephyr.bin.dfu
	@rm -f outdir/arduino_101/zephyr.bin.dfu
	@rm -f outdir/jsgen.tmp
	@rm -f outdir/include/zjs_script_gen.h
	@rm -f outdir/include/zjs_snapshot_gen.h
	@rm -f prj.conf
	@rm -f prj.conf.tmp
	@rm -f prj.mdef
	@rm -f zjs.conf.tmp
	@rm -f js.tmp

# Explicit clean
.PHONY: clean
clean: cleanlocal
ifeq ($(BOARD), linux)
	@make -f Makefile.linux clean
else
	@rm -rf $(JERRY_BASE)/build/$(BOARD)/;
	@rm -f outdir/$(BOARD)/libjerry-core*.a;
	@make -f Makefile.zephyr clean BOARD=$(BOARD);
	@cd arc/; make clean;
endif

.PHONY: pristine
pristine: cleanlocal
	@rm -rf $(JERRY_BASE)/build;
	@make -f Makefile.zephyr pristine;
	@cd arc; make pristine;

# Generate the script file from the JS variable
.PHONY: generate
generate: $(JS) setup
	@mkdir -p outdir/include/
ifeq ($(SNAPSHOT), on)
	@echo Building snapshot generator...
	@if ! [ -e outdir/snapshot/snapshot ]; then \
		make -f tools/Makefile.snapshot; \
	fi
	@echo Creating snapshot bytecode from JS application...
	@if [ -x /usr/bin/uglifyjs ]; then \
		uglifyjs js.tmp -nc -mt > outdir/jsgen.tmp; \
	else \
		cat js.tmp > outdir/jsgen.tmp; \
	fi
	@outdir/snapshot/snapshot outdir/jsgen.tmp > outdir/include/zjs_snapshot_gen.h
else
	@echo Creating C string from JS application...
ifeq ($(BOARD), linux)
	@./scripts/convert.sh $(JS) outdir/include/zjs_script_gen.h
else
	@./scripts/convert.sh js.tmp outdir/include/zjs_script_gen.h
endif
endif

NET_BUILD=$(shell grep -q -E "BUILD_MODULE_OCF|BUILD_MODULE_DGRAM|BUILD_MODULE_NET|BUILD_MODULE_WS" src/Makefile && echo y)

# Run QEMU target
.PHONY: qemu
qemu: zephyr
	make -f Makefile.zephyr qemu \
		CB_STATS=$(CB_STATS) \
		SNAPSHOT=$(SNAPSHOT) \
		NETWORK_BUILD=$(NET_BUILD) \
		ZJS_FLAGS=$(ZJS_FLAGS)

# Builds ARC binary
.PHONY: arc
arc: analyze
	@echo "# This is a generated file" > arc/prj.conf
	@cat arc/fragments/prj.conf.base >> arc/prj.conf
	@if [ -e arc/prj.conf.tmp ]; then \
		cat arc/prj.conf.tmp >> arc/prj.conf; \
	fi
	@echo "# This is a generated file" > arc/src/Makefile
	@cat arc/src/Makefile.base >> arc/src/Makefile
	@echo "ccflags-y += $(shell ./scripts/analyze.sh $(BOARD) $(JS) $(CONFIG) $(ASHELL))" >> arc/src/Makefile
	@printf "CONFIG_SRAM_SIZE=%d\n" $$((79 - $(RAM))) >> arc/prj.conf
	@printf "CONFIG_FLASH_BASE_ADDRESS=0x400%x\n" $$((($(ROM) + 64) * 1024)) >> arc/prj.conf
	@if [ "$(OS)" = "Darwin" ]; then \
		sed -i.bu '/This is a generated file/r./zjs.conf.tmp' arc/src/Makefile; \
		cd arc; make BOARD=arduino_101_sss CROSS_COMPILE=$(ARC_CROSS_COMPILE); \
	else \
		sed -i '/This is a generated file/r./zjs.conf.tmp' arc/src/Makefile; \
		cd arc; make BOARD=arduino_101_sss; \
	fi
ifeq ($(BOARD), arduino_101)
	@echo
ifeq ($(OS), Darwin)
	@if test $$(((296 - $(ROM)) * 1024)) -lt $$(stat -f "%z" $(A101SSBIN)); then echo Error: ARC image \($$(stat -f "%z" $(A101SSBIN)) bytes\) will not fit in available $$(((296 - $(ROM))))KB space. Try decreasing ROM.; return 1; fi
else
	@if test $$(((296 - $(ROM)) * 1024)) -lt $$(stat --printf="%s" $(A101SSBIN)); then echo Error: ARC image \($$(stat --printf="%s" $(A101SSBIN)) bytes\) will not fit in available $$(((296 - $(ROM))))KB space. Try decreasing ROM.; return 1; fi
endif
endif

# Run debug server over JTAG
.PHONY: debug
debug:
	make -f Makefile.zephyr BOARD=arduino_101 ARCH=x86 debugserver

# Run gdb to connect to debug server for x86
.PHONY: gdb
gdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/i686-pokysdk-linux/usr/bin/iamcu-poky-elfiamcu/i586-poky-elfiamcu-gdb outdir/arduino_101/zephyr.elf -ex "target remote :3333"

# Run gdb to connect to debug server for ARC
.PHONY: arcgdb
arcgdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/i686-pokysdk-linux/usr/bin/arc-poky-elf/arc-poky-elf-gdb arc/outdir/zephyr.elf -ex "target remote :3334"

# Linux target
.PHONY: linux
# Linux command line target, script can be specified on the command line
linux: generate
	make -f Makefile.linux JS=$(JS) VARIANT=$(VARIANT) CB_STATS=$(CB_STATS) V=$(V) SNAPSHOT=$(SNAPSHOT)

.PHONY: help
help:
	@echo "JavaScript Runtime for Zephyr OS - Build System"
	@echo
	@echo "Build targets:"
	@echo "    all:        Build for either Zephyr or Linux depending on BOARD"
	@echo "    zephyr:     Build Zephyr for the given BOARD (A101 is default)"
	@echo "    ide         Build Zephyr in development mode for the IDE"
	@echo "    ashell      Build Zephyr in development mode for command line"
	@echo "    dfu:        Flash x86 and arc images to A101 with dfu-util"
	@echo "    debug:      Run debug server using JTAG"
	@echo "    gdb:        Run gdb to connect to debug server for x86"
	@echo "    qemu:       Run QEMU after building"
	@echo "    clean:      Clean stale build objects for given BOARD"
	@echo "    pristine:   Completely remove all generated files"
	@echo "    check:      Run all the automated build tests"
	@echo "    quickcheck: Run the quick Linux subset of automated build tests"
	@echo
	@echo "Build options:"
	@echo "    BOARD=      Specify a Zephyr board to build for"
	@echo "    JS=         Specify a JS script to compile into the binary"
	@echo "    RAM=        Specify size in KB for RAM allocated to X86"
	@echo "    ROM=        Specify size in KB for X86 partition (144 - 296)"
	@echo "    SNAPSHOT=   Specify off to turn off snapshotting"
	@echo "    TRACE=      Specify 'on' for malloc tracing (off is default)"
	@echo "    VARIANT=    Specify 'debug' for extra serial output detail"
	@echo
