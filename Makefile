# Copyright (c) 2016-2017, Intel Corporation.

# a place to add temporary defines to ZJS builds such as -DZJS_GPIO_MOCK
ZJS_FLAGS :=

# control verbosity of debug prints: 1 = text, 2 = func/line, 3 = timestamp
VERBOSITY ?= 1

OS := $(shell uname)

BOARD ?= arduino_101
RAM ?= 64
ROM ?= 144
V ?= 0
JS_TMP = js.tmp

# Dump memory information: on = print allocs, full = print allocs
TRACE ?= off

# Enable jerry-debugger, currently only work on linux
DEBUGGER ?= off

# ALL-IN-ONE build, slightly shrink build size, may not work on all platforms
ALL_IN_ONE ?= off

ifeq (,$(O))
O := outdir
endif

OUT := $(abspath $(O))
$(info Using outdir: $(OUT))

ifneq (,$(DEV))
$(error DEV= is no longer supported, please use make ide or make ashell)
endif

ifndef ZJS_BASE
$(error ZJS_BASE not defined. You need to source zjs-env.sh)
endif

ifeq ($(DEBUGGER), on)
ifneq ($(BOARD), linux)
$(error Debugger only runs on linux, set BOARD=linux)
endif
ifneq ($(SNAPSHOT), on)
$(warning Debugger on, disabling snapshot)
SNAPSHOT=off
endif
endif

ifeq ($(BOARD), arduino_101)
ALL_IN_ONE=on
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

# IDE_GPIO_PIN has to be between 0 to 20
ifneq ($(IDE_GPIO_PIN),)
ifeq ($(shell test $(IDE_GPIO_PIN) -lt 0; echo $$?), 0)
$(error IDE_GPIO_PIN must be no lower than 0)
endif
ifeq ($(shell test $(IDE_GPIO_PIN) -gt 20; echo $$?), 0)
$(error IDE_GPIO_PIN must be no higher than 20)
endif
endif
endif  # BOARD = arduino_101

ifeq ($(filter $(MAKECMDGOALS),linux), linux)
$(error 'linux' make target is deprecated, use "make BOARD=linux")
endif

ifneq (,$(filter $(MAKECMDGOALS),ide ashell))
ifneq (,$(JS))
$(error ide and ashell do not allow for setting JS)
endif
endif

ifeq ($(filter $(MAKECMDGOALS),ide), ide)
ASHELL_TYPE=ide
endif
ifeq ($(filter $(MAKECMDGOALS),ashell), ashell)
ASHELL_TYPE=cli
endif

OCF_ROOT ?= deps/iotivity-constrained
JS ?= samples/HelloWorld.js
VARIANT ?= release
DEVICE_NAME ?= "ZJS Device"
BLE_ADDR ?= "none"
FUNC_NAME ?= off
# JerryScript options
JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript
EXT_JERRY_FLAGS ?=	-DENABLE_ALL_IN_ONE=$(ALL_IN_ONE) \
			-DFEATURE_INIT_FINI=ON \
			-DFEATURE_PROFILE=$(OUT)/$(BOARD)/jerry_feature.profile \
			-DFEATURE_ERROR_MESSAGES=ON \
			-DJERRY_LIBM=OFF \
			-DJERRY_PORT_DEFAULT=OFF

# Work-around for #2363 until Zephyr fixes the SDK
ifeq ($(VARIANT), release)
KBUILD_CFLAGS_OPTIMIZE = -O2
endif

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
ZJS_FLAGS := $(ZJS_FLAGS) -DZJS_FIND_FUNC_NAME
endif

ifeq ($(FORCE),)
FORCED := zjs_common.json
else
FORCED := $(FORCE),zjs_common.json
endif

# Settings for ashell builds
ifneq (,$(filter $(MAKECMDGOALS),ide ashell))
ASHELL=zjs_ashell_$(ASHELL_TYPE).json
FORCED := $(ASHELL),$(FORCED)
ASHELL_ARC=zjs_ashell_arc.json
ZJS_FLAGS += -DZJS_FIND_FUNC_NAME
ifneq ($(IDE_GPIO_PIN),)
ZJS_FLAGS += -DIDE_GPIO_PIN=$(IDE_GPIO_PIN)
endif
endif

ifeq ($(BOARD), arduino_101)
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

A101BIN = $(OUT)/arduino_101/zephyr.bin
A101SSBIN = $(OUT)/arduino_101_sss/zephyr.bin

.PHONY: ram_report
ram_report: zephyr
	@make -f Makefile.zephyr	BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					ZJS_FLAGS="$(ZJS_FLAGS)" \
					O=$(OUT)/$(BOARD) \
					ram_report

.PHONY: rom_report
rom_report: zephyr
	@make -f Makefile.zephyr	BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					ZJS_FLAGS="$(ZJS_FLAGS)" \
					O=$(OUT)/$(BOARD) \
					rom_report

# choose name of jerryscript library based on snapshot feature
ifeq ($(SNAPSHOT), on)
JERRYLIB=$(OUT)/$(BOARD)/libjerry-core-snapshot.a
else
JERRYLIB=$(OUT)/$(BOARD)/libjerry-core-parser.a
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
		O=$(OUT)/$(BOARD) \
		NETWORK_BUILD=$(NET_BUILD)

# Build for zephyr, default target
.PHONY: zephyr
zephyr: analyze generate $(JERRYLIB) $(OUT)/$(BOARD)/libjerry-ext.a $(ARC)
	@make -f Makefile.zephyr -j1 \
					BOARD=$(BOARD) \
					VARIANT=$(VARIANT) \
					VERBOSITY=$(VERBOSITY) \
					CB_STATS=$(CB_STATS) \
					PRINT_FLOAT=$(PRINT_FLOAT) \
					SNAPSHOT=$(SNAPSHOT) \
					BLE_ADDR=$(BLE_ADDR) \
					ASHELL=$(ASHELL) \
					NETWORK_BUILD=$(NET_BUILD) \
					O=$(OUT)/$(BOARD) \
					ZJS_FLAGS="$(ZJS_FLAGS)"
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
	mkdir -p $(OUT)/$(BOARD)/
	cp $(JERRY_BASE)/build/$(BOARD)/obj-$(BOARD)/lib/libjerry-core.a $(JERRYLIB)
	cp $(JERRY_BASE)/build/$(BOARD)/obj-$(BOARD)/lib/libjerry-ext.a $(OUT)/$(BOARD)/

# Give an error if we're asked to create the JS file
$(JS):
	$(error The file $(JS) does not exist.)

# Find the modules the JS file depends on
.PHONY: analyze
analyze: $(JS)
	@mkdir -p $(OUT)/$(BOARD)/
	@mkdir -p $(OUT)/include
	@echo "% This is a generated file" > prj.mdef

	./scripts/analyze	V=$(V) \
		SCRIPT=$(JS) \
		JS_OUT=$(JS_TMP) \
		BOARD=$(BOARD) \
		JSON_DIR=src/ \
		O=$(OUT) \
		FORCE=$(FORCED) \
		PRJCONF=prj.conf \
		MAKEFILE=src/Makefile \
		MAKEBASE=src/Makefile.base \
		PROFILE=$(OUT)/$(BOARD)/jerry_feature.profile

	@if [ "$(TRACE)" = "on" ] || [ "$(TRACE)" = "full" ]; then \
		echo "ccflags-y += -DZJS_TRACE_MALLOC" >> src/Makefile; \
	fi
	@if [ "$(SNAPSHOT)" = "on" ]; then \
		echo "ccflags-y += -DZJS_SNAPSHOT_BUILD" >> src/Makefile; \
	fi
	@# Add bluetooth debug configs if BLE is enabled
	@if grep -q BUILD_MODULE_BLE src/Makefile; then \
		if [ "$(VARIANT)" = "debug" ]; then \
			echo "CONFIG_BT_DEBUG_LOG=y" >> prj.conf; \
		fi \
	fi

	@if grep -q BUILD_MODULE_OCF src/Makefile; then \
		echo "CONFIG_BT_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf; \
	fi
	@if grep -q BUILD_MODULE_DGRAM src/Makefile; then \
		echo "CONFIG_BT_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf; \
	fi
	@if [ -e $(OUT)/$(BOARD)/jerry_feature.profile.bak ]; then \
		if ! cmp $(OUT)/$(BOARD)/jerry_feature.profile.bak $(OUT)/$(BOARD)/jerry_feature.profile; \
		then \
			rm -f $(OUT)/$(BOARD)/libjerry-core*.a; \
			rm -f $(OUT)/$(BOARD)/libjerry-ext*.a; \
		fi \
	else \
		rm -f $(OUT)/$(BOARD)/libjerry-core*.a; \
		rm -f $(OUT)/$(BOARD)/libjerry-ext*.a; \
	fi

# Update dependency repos
.PHONY: update
update:
	@git submodule update --init
	@cd $(OCF_ROOT); git submodule update --init;

# set up prj.conf file
-.PHONY: setup
setup:
ifeq ($(BOARD), qemu_x86)
ifneq ($(OS), Darwin)
	echo "CONFIG_XIP=y" >> prj.conf
else
	echo "CONFIG_RAM_SIZE=192" >> prj.conf
endif
else
ifeq ($(ASHELL), ashell)
	@cat fragments/prj.mdef.ashell >> prj.mdef
ifeq ($(filter ide,$(MAKECMDGOALS)),ide)
	@echo CONFIG_USB_CDC_ACM=n >> prj.conf
else
	@echo CONFIG_USB_CDC_ACM=y >> prj.conf
endif
endif
ifeq ($(BOARD), arduino_101)
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
endif

.PHONY: cleanlocal
cleanlocal:
	@rm -f src/*.o
	@rm -f src/Makefile
	@rm -f arc/prj.conf
	@rm -f arc/prj.conf.tmp
	@rm -f arc/src/Makefile
	@rm -f prj.conf
	@rm -f prj.conf.tmp
	@rm -f prj.mdef
	@rm -f zjs.conf.tmp
	@rm -f $(JS_TMP)

# Explicit clean
.PHONY: clean
clean: cleanlocal
ifeq ($(BOARD), linux)
	@make -f Makefile.linux O=$(OUT)/$(BOARD) clean
else
	@rm -rf $(JERRY_BASE)/build/$(BOARD)/;
	@rm -f $(OUT)/$(BOARD)/libjerry-core*.a;
	@rm -f $(OUT)/$(BOARD)/libjerry-ext*.a;
	@make -f Makefile.zephyr BOARD=$(BOARD) O=$(OUT)/$(BOARD) clean;
	@cd arc/; make clean;
endif

.PHONY: pristine
pristine: cleanlocal
	@rm -rf $(JERRY_BASE)/build;
	@make -f Makefile.zephyr O=$(OUT) pristine;
	@cd arc; make pristine;

# Generate the script file from the JS variable
.PHONY: generate
generate: $(JS) setup
	@mkdir -p $(OUT)/include/
ifeq ($(SNAPSHOT), on)
	@echo Building snapshot generator...
	@if ! [ -e $(OUT)/snapshot/snapshot ]; then \
		make -f tools/Makefile.snapshot O=$(OUT); \
	fi
	@echo Creating snapshot bytecode from JS application...
	@if [ -x /usr/bin/uglifyjs ]; then \
		uglifyjs $(JS_TMP) -nc -mt > $(OUT)/jsgen.tmp; \
	else \
		cat $(JS_TMP) > $(OUT)/jsgen.tmp; \
	fi
	@$(OUT)/snapshot/snapshot $(OUT)/jsgen.tmp > $(OUT)/include/zjs_snapshot_gen.h
else
	@echo Creating C string from JS application...
ifeq ($(BOARD), linux)
	@./scripts/convert.sh $(JS) $(OUT)/include/zjs_script_gen.h
else
	@./scripts/convert.sh $(JS_TMP) $(OUT)/include/zjs_script_gen.h
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
		O=$(OUT)/qemu_x86 \
		ZJS_FLAGS="$(ZJS_FLAGS)"

ARC_RESTRICT="zjs_ipm_arc.json,\
		zjs_i2c_arc.json,\
		zjs_arc.json,\
		zjs_pme_arc.json,\
		zjs_aio_arc.json,\
		zjs_ashell_arc.json,\
		zjs_sensor_arc.json,\
		zjs_sensor_accel_arc.json,\
		zjs_sensor_light_arc.json,\
		zjs_sensor_gyro_arc.json,\
		zjs_sensor_temp_arc.json"

# Builds ARC binary
.PHONY: arc
arc: analyze
	# Restrict ARC build to only certain "arc specific" modules
	./scripts/analyze	V=$(V) \
		SCRIPT=$(JS_TMP) \
		BOARD=arc \
		JSON_DIR=arc/src/ \
		PRJCONF=arc/prj.conf \
		MAKEFILE=arc/src/Makefile \
		MAKEBASE=arc/src/Makefile.base \
		O=$(OUT)/arduino_101_sss \
		FORCE=$(ASHELL_ARC)

	@printf "CONFIG_SRAM_SIZE=%d\n" $$((79 - $(RAM))) >> arc/prj.conf
	@printf "CONFIG_FLASH_BASE_ADDRESS=0x400%x\n" $$((($(ROM) + 64) * 1024)) >> arc/prj.conf
	@if [ "$(OS)" = "Darwin" ]; then \
		sed -i.bu '/This is a generated file/r./zjs.conf.tmp' arc/src/Makefile; \
		cd arc; make BOARD=arduino_101_sss CROSS_COMPILE=$(ARC_CROSS_COMPILE) O=$(OUT)/arduino_101_sss; \
	else \
		sed -i '/This is a generated file/r./zjs.conf.tmp' arc/src/Makefile; \
		cd arc; make BOARD=arduino_101_sss KBUILD_CFLAGS_OPTIMIZE=$(KBUILD_CFLAGS_OPTIMIZE) O=$(OUT)/arduino_101_sss -j1; \
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
	make -f Makefile.zephyr BOARD=arduino_101 ARCH=x86 O=$(OUT)/arduino_101 debugserver

# Run gdb to connect to debug server for x86
.PHONY: gdb
gdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/x86_64-pokysdk-linux/usr/bin/i586-zephyr-elfiamcu/i586-zephyr-elfiamcu-gdb $(OUT)/arduino_101/zephyr.elf -ex "target remote :3333"

# Run gdb to connect to debug server for ARC
.PHONY: arcgdb
arcgdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/i686-pokysdk-linux/usr/bin/arc-poky-elf/arc-poky-elf-gdb $(OUT)/arduino_101_sss/zephyr.elf -ex "target remote :3334"

# Linux target
.PHONY: linux
# Linux command line target, script can be specified on the command line
linux: generate
	make -f Makefile.linux JS=$(JS) VARIANT=$(VARIANT) CB_STATS=$(CB_STATS) V=$(V) SNAPSHOT=$(SNAPSHOT) DEBUGGER=$(DEBUGGER) O=$(OUT)/linux

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
