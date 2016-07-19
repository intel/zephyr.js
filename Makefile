BOARD ?= arduino_101_factory
KERNEL ?= micro

ifndef JERRY_BASE
$(error JERRY_BASE not defined)
endif

.PHONY: all
all: zephyr arc

# Check if a clean is needed before building
ifneq ("$(wildcard .$(BOARD).last_build)", "")
PRE_ACTION=
else
PRE_ACTION=clean
endif

# Update target, reads repos.txt and updated exising entries
.PHONY: update
update: setup
	@cd deps/; \
	while read line; do \
		case \
			"$$line" in \#*) \
				continue ;; \
			*) \
				name=$$(echo $$line | cut -d ' ' -f 1); \
				commit=$$(echo $$line | cut -d ' ' -f 3); \
				cd $$name; \
				git checkout $$commit; \
				cd ..; \
				continue ;; \
		esac \
	done < repos.txt

# Sets up dependencies using deps/repos.txt
.PHONY: setup
setup:
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
					echo $$commit; \
					git clone $$repo $$name; \
					cd $$name; \
					git checkout $$commit; \
					cd ..; \
				fi; \
				continue ;; \
		esac \
	done < repos.txt
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

# Explicit clean
.PHONY: clean
clean:
	@if [ -d deps/zephyr ]; then \
		make -f Makefile.zephyr clean; \
		make -C $(JERRY_BASE) -f targets/zephyr/Makefile.zephyr clean; \
		make -C $(JERRY_BASE) -f targets/zephyr/Makefile clean; \
	fi
	@if [ -d deps/jerryscript ]; then \
		rm -rf deps/jerryscript/build/$(BOARD)/; \
	fi

# Arduino 101 flash target
.PHONY: flash
flash:
	dfu-util -a x86_app -D outdir/zephyr.bin

# Generate the script file from the JS variable
ifdef JS
.PHONY: generate
generate: setup $(PRE_ACTION)
	./scripts/convert.sh $(JS) src/zjs_script_gen.c
endif

# Run QEMU target
.PHONY: qemu
ifdef JS
qemu: generate
else
qemu: setup $(PRE_ACTION)
endif
	make -f Makefile.zephyr BOARD=qemu_x86 KERNEL=$(KERNEL) qemu

# Build for zephyr, default target
.PHONY: zephyr
ifdef JS
zephyr: generate
else
zephyr: setup $(PRE_ACTION)
endif
	make -f Makefile.zephyr BOARD=$(BOARD) KERNEL=$(KERNEL)

# Builds ARC binary
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
	@echo "    setup:     Sets up dependencies (ran by default)"
	@echo "    update:    Updates dependencies"
	@echo
	@echo "Build options:"
	@echo "    BOARD=     Specify a Zephyr board to build for"
	@echo "    JS=        Specify a JS script to compile into the binary"
	@echo "    KERNEL=    Specify the kernel to use (micro or nano)"
