# Copyright (c) 2016-2017, Intel Corporation.

# a place to add temporary defines to ZJS builds such as -DZJS_GPIO_MOCK
#ZJS_FLAGS :=

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
BLE_ADDR ?= none
FUNC_NAME ?= off
# JerryScript options
JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript
JERRY_OUTPUT = $(OUT)/$(BOARD)/jerry/build

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
endif

ifeq ($(FUNC_NAME), on)
ZJS_FLAGS += -DZJS_FIND_FUNC_NAME
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
ARC = arc
ARC_RAM = $$((79 - $(RAM)))
ARC_ROM = $$((296 - $(ROM)))
PHYS_RAM_ADDR = $(shell printf "%x" $$(((80 - $(RAM)) * 1024)))
FLASH_BASE_ADDR = $(shell printf "%x" $$((($(ROM) + 64) * 1024)))
ifeq ($(MAKECMDGOALS),)
TARGETS=all
else
TARGETS=$(MAKECMDGOALS)
endif

# if actually building for A101 (not clean), report RAM/ROM allocation
ifneq ($(filter all zephyr arc,$(TARGETS)),)
$(info Building for Arduino 101...)
$(info $() $() RAM allocation: $(RAM)KB for X86, $(shell echo $(ARC_RAM))KB for ARC)
$(info $() $() ROM allocation: $(ROM)KB for X86, $(shell echo $(ARC_ROM))KB for ARC)
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

A101BIN = $(OUT)/arduino_101/zephyr/zephyr.bin
A101SSBIN = $(OUT)/arduino_101_sss/zephyr/zephyr.bin

.PHONY: ram_report
ram_report: zephyr
	@make -C $(OUT)/$(BOARD) ram_report

.PHONY: rom_report
rom_report: zephyr
	@make -C $(OUT)/$(BOARD) rom_report

.PHONY: flash
flash:
	@make -C $(OUT)/$(BOARD) flash

# Build for zephyr, default target
.PHONY: zephyr
zephyr: analyze generate $(ARC)
	@cmake $(CMAKEFLAGS) -H. && \
	make -C $(OUT)/$(BOARD) -j4
ifeq ($(BOARD), arduino_101)
	@echo
	@echo -n Creating dfu images...
	@dd if=$(A101BIN) of=$(A101BIN).dfu bs=1024 count=144 2> /dev/null
	@dd if=$(A101BIN) of=$(A101SSBIN).dfu bs=1024 skip=144 2> /dev/null
	@dd if=$(A101SSBIN) of=$(A101SSBIN).dfu bs=1024 seek=$$(($(ROM) - 144)) 2> /dev/null
	@echo " done."
	@cp -p $(BOARD).overlay $(OUT)/$(BOARD)/$(BOARD).overlay.bak
endif

.PHONY: ide
ide: zephyr

.PHONY: ashell
ashell: zephyr

# Flash images
.PHONY: dfu
dfu:
ifeq ($(BOARD), arduino_101)
	dfu-util -a x86_app -D $(A101BIN).dfu
	dfu-util -a sensor_core -D $(A101SSBIN).dfu
endif
ifeq ($(BOARD), olimex_stm32_e407)
	dfu-util -a 0 -d 0483:df11 -D $(OUT)/olimex_stm32_e407/zephyr/zephyr.bin --dfuse-address 0x08000000
endif
ifeq ($(BOARD), 96b_carbon)
	dfu-util -a 0 -d 0483:df11 -D $(OUT)/96b_carbon/zephyr/zephyr.bin --dfuse-address 0x08000000
endif

# Give an error if we're asked to create the JS file
$(JS):
	$(error The file $(JS) does not exist.)

# Find the modules the JS file depends on
.PHONY: analyze
analyze: $(JS)
	@if [ -e $(OUT)/$(BOARD)/$(BOARD).overlay.bak ]; then \
		if ! cmp $(OUT)/$(BOARD)/$(BOARD).overlay.bak $(BOARD).overlay; then \
			echo "RAM/ROM size change detected in overlay, rebuild..."; \
			rm -rf $(OUT)/$(BOARD)/zephyr; \
			rm -rf $(OUT)/$(BOARD)/deps; \
		fi \
	fi
	@if [ -e prj.conf ]; then \
		if ! grep -q CONFIG_ROM_SIZE=$(ROM) prj.conf || ! grep CONFIG_RAM_SIZE=$(RAM) prj.conf; then \
			echo "RAM/ROM size change detected in prj.conf, rebuild..."; \
			rm -rf $(OUT)/$(BOARD)/zephyr; \
			rm -rf $(OUT)/$(BOARD)/deps; \
		fi \
	fi
	@mkdir -p $(OUT)/$(BOARD)/
	@mkdir -p $(OUT)/include

	./scripts/analyze	--verbose=$(V) \
		--script=$(JS) \
		--js-out=$(OUT)/$(JS_TMP) \
		--board=$(BOARD) \
		--json-dir=src/ \
		--output-dir=$(OUT) \
		--force=$(FORCED) \
		--prjconf=prj.conf \
		--cmakefile=$(OUT)/$(BOARD)/generated.cmake \
		--profile=$(OUT)/$(BOARD)/jerry_feature.profile

	@if [ "$(TRACE)" = "on" ] || [ "$(TRACE)" = "full" ]; then \
		echo "add_definitions(-DZJS_TRACE_MALLOC)" >> $(OUT)/$(BOARD)/generated.cmake; \
	fi
	@if [ "$(SNAPSHOT)" = "on" ]; then \
		echo "add_definitions(-DZJS_SNAPSHOT_BUILD)" >> $(OUT)/$(BOARD)/generated.cmake; \
	fi
	@# Build NEWLIB with float print support, this will increase ROM size
	@if [ "$(PRINT_FLOAT)" = "on" ]; then \
		echo "CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y" >> prj.conf; \
	fi
	@# Add bluetooth debug configs if BLE is enabled
	@if grep -q BUILD_MODULE_BLE $(OUT)/$(BOARD)/generated.cmake; then \
		if [ "$(VARIANT)" = "debug" ]; then \
			echo "CONFIG_BT_DEBUG_LOG=y" >> prj.conf; \
		fi \
	fi
	@if grep -q BUILD_MODULE_OCF $(OUT)/$(BOARD)/generated.cmake; then \
		echo "CONFIG_BT_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf; \
	fi
	@if grep -q BUILD_MODULE_DGRAM $(OUT)/$(BOARD)/generated.cmake; then \
		echo "CONFIG_BT_DEVICE_NAME=\"$(DEVICE_NAME)\"" >> prj.conf; \
	fi
	@if [ -e $(OUT)/$(BOARD)/jerry_feature.profile.bak ]; then \
		if ! cmp $(OUT)/$(BOARD)/jerry_feature.profile.bak $(OUT)/$(BOARD)/jerry_feature.profile; then \
			echo "JerryScript profile changed, rebuild..."; \
			rm -rf $(OUT)/$(BOARD)/zephyr; \
			rm -rf $(OUT)/$(BOARD)/deps; \
		fi \
	fi

	$(eval CMAKEFLAGS = \
		-B$(OUT)/$(BOARD) \
		-DASHELL=$(ASHELL) \
		-DBLE_ADDR=$(BLE_ADDR) \
		-DBOARD=$(BOARD) \
		-DCB_STATS=$(CB_STATS) \
		-DJERRY_BASE=$(JERRY_BASE) \
		-DJERRY_OUTPUT=$(JERRY_OUTPUT) \
		-DJERRY_PROFILE=$(OUT)/$(BOARD)/jerry_feature.profile \
		-DPRINT_FLOAT=$(PRINT_FLOAT) \
		-DSNAPSHOT=$(SNAPSHOT) \
		-DVARIANT=$(VARIANT) \
		-DVERBOSITY=$(VERBOSITY) \
		-DZJS_FLAGS="$(ZJS_FLAGS)")
	$(info CMAKEFLAGS = $(CMAKEFLAGS))

# Update dependency repos
.PHONY: update
update:
	@git submodule update --init
	@cd $(OCF_ROOT); git submodule update --init;

# set up prj.conf file
-.PHONY: setup
setup:
ifeq ($(ASHELL), ashell)
ifeq ($(filter ide,$(MAKECMDGOALS)),ide)
	@echo CONFIG_USB_CDC_ACM=n >> prj.conf
else
	@echo CONFIG_USB_CDC_ACM=y >> prj.conf
endif
endif
ifeq ($(BOARD), arduino_101)
ifeq ($(OS), Darwin)
	@# work around for OSX where the xtool toolchain do not
	@# support iamcu instruction set on the Arduino 101
	@echo "CONFIG_X86_IAMCU=n" >> prj.conf
endif
	@echo "CONFIG_RAM_SIZE=$(RAM)" >> prj.conf
	@echo "CONFIG_ROM_SIZE=$(ROM)" >> prj.conf
	@printf "CONFIG_SS_RESET_VECTOR=0x400%x\n" $$((($(ROM) + 64) * 1024)) >> prj.conf
	@echo "&flash0 { reg = <0x40010000 ($(ROM) * 1024)>; };" > arduino_101.overlay
	@echo "&flash1 { reg = <0x40030000 ($(ROM) * 1024)>; };" >> arduino_101.overlay
	@echo "&sram0 { reg = <0xa800$(PHYS_RAM_ADDR) ($(RAM) * 1024)>; };" >> arduino_101.overlay
endif

.PHONY: cleanlocal
cleanlocal:
	@rm -f $(OUT)/$(BOARD)/generated.cmake
	@rm -f arc/arduino_101_sss.overlay
	@rm -f arc/prj.conf
	@rm -f arc/prj.conf.tmp
	@rm -f arduino_101.overlay
	@rm -f prj.conf
	@rm -f prj.conf.tmp
	@rm -f $(OUT)/$(JS_TMP)

# Explicit clean
.PHONY: clean
clean: cleanlocal
	@rm -rf $(JERRY_BASE)/build/$(BOARD)/;
	@rm -rf $(OUT)/$(BOARD)/
	@rm -f $(OUT)/jsgen.tmp

.PHONY: pristine
pristine: cleanlocal
	@rm -rf $(JERRY_BASE)/build;
	@rm -rf $(OUT)

# Generate the script file from the JS variable
.PHONY: generate
generate: $(JS) setup
	@mkdir -p $(OUT)/include/
	@# create an config.h file to needed for iotivity-constrained
	@cp -p src/zjs_ocf_config.h $(OUT)/include/config.h
ifeq ($(SNAPSHOT), on)
	@echo Building snapshot generator...
	@if ! [ -e $(OUT)/snapshot/snapshot ]; then \
		make -f tools/Makefile.snapshot O=$(OUT); \
	fi
	@echo Creating snapshot bytecode from JS application...
	@if [ -x /usr/bin/uglifyjs ]; then \
		uglifyjs $(OUT)/$(JS_TMP) -nc -mt > $(OUT)/jsgen.tmp; \
	else \
		cat $(OUT)/$(JS_TMP) > $(OUT)/jsgen.tmp; \
	fi
	@$(OUT)/snapshot/snapshot $(OUT)/jsgen.tmp > $(OUT)/include/zjs_snapshot_gen.h
else
	@echo Creating C string from JS application...
ifeq ($(BOARD), linux)
	@./scripts/convert.py $(JS) $(OUT)/include/zjs_script_gen.h
else
	@./scripts/convert.py $(OUT)/$(JS_TMP) $(OUT)/include/zjs_script_gen.h
endif
endif

# Run QEMU target
.PHONY: qemu
qemu: zephyr
	@make -C $(OUT)/qemu_x86 run

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
	@# Restrict ARC build to only certain "arc specific" modules
	@mkdir -p $(OUT)/arduino_101_sss
	./scripts/analyze	--verbose=$(V) \
		--script=$(OUT)/$(JS_TMP) \
		--board=arc \
		--json-dir=arc/src/ \
		--prjconf=arc/prj.conf \
		--cmakefile=$(OUT)/arduino_101_sss/generated.cmake \
		--output-dir=$(OUT)/arduino_101_sss \
		--force=$(ASHELL_ARC)

	@echo "&flash0 { reg = <0x400$(FLASH_BASE_ADDR) ($(ARC_ROM) * 1024)>; };" > arc/arduino_101_sss.overlay
	@echo "&sram0 { reg = <0xa8000400 ($(ARC_RAM) * 1024)>; };" >> arc/arduino_101_sss.overlay
	@cmake -B$(OUT)/arduino_101_sss \
		-DBOARD=arduino_101_sss \
		-DVARIANT=$(VARIANT) \
		-H./arc && \
	make -C $(OUT)/arduino_101_sss -j4
ifeq ($(BOARD), arduino_101)
	@echo
ifeq ($(OS), Darwin)
	@if test $$(((296 - $(ROM)) * 1024)) -lt $$(stat -f "%z" $(A101SSBIN)); then echo Error: ARC image \($$(stat -f "%z" $(A101SSBIN)) bytes\) will not fit in available $$(((296 - $(ROM))))KB space. Try decreasing ROM.; return 1; fi
else
	@if test $$(((296 - $(ROM)) * 1024)) -lt $$(stat --printf="%s" $(A101SSBIN)); then echo Error: ARC image \($$(stat --printf="%s" $(A101SSBIN)) bytes\) will not fit in available $$(((296 - $(ROM))))KB space. Try decreasing ROM.; return 1; fi
endif
endif

# Run debug server over JTAG
.PHONY: adebug
adebug:
	@make -C $(OUT)/$(BOARD) debugserver

# Run gdb to connect to debug server for x86
.PHONY: agdb
agdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/x86_64-pokysdk-linux/usr/bin/i586-zephyr-elfiamcu/i586-zephyr-elfiamcu-gdb $(OUT)/arduino_101/zephyr/zephyr.elf -ex "target remote :3333"

# Run gdb to connect to debug server for ARC
.PHONY: arcgdb
arcgdb:
	$$ZEPHYR_SDK_INSTALL_DIR/sysroots/i686-pokysdk-linux/usr/bin/arc-poky-elf/arc-poky-elf-gdb $(OUT)/arduino_101_sss/zephyr/zephyr.elf -ex "target remote :3334"

# Linux target
.PHONY: linux
# Linux command line target, script can be specified on the command line
linux: generate
	@cmake -B$(OUT)/linux \
		-DBOARD=linux \
		-DCB_STATS=$(CB_STATS) \
		-DDEBUGGER=$(DEBUGGER) \
		-DV=$(V) \
		-DVARIANT=$(VARIANT) \
		-H. && \
	make -C $(OUT)/linux;

.PHONY: help
help:
	@echo "JavaScript Runtime for Zephyr OS - Build System"
	@echo
	@echo "Build targets:"
	@echo "    all:        Build for either Zephyr or Linux depending on BOARD"
	@echo "    zephyr:     Build Zephyr for the given BOARD (A101 is default)"
	@echo "    ide         Build Zephyr in development mode for the IDE"
	@echo "    ashell      Build Zephyr in development mode for command line"
	@echo "    debug:      Run Zephyr debug target"
	@echo "    flash:      Run Zephyr flash target"
	@echo "    dfu:        Flash x86 and arc images to A101 with dfu-util"
	@echo "    adebug:     Run debug server for A101 using JTAG"
	@echo "    agdb:       Run gdb to connect to A101 debug server"
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
