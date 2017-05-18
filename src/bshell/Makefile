# Copyright (c) 2016-2017, Intel Corporation.

ifdef V
$(info Compiling application)
endif

# Adding path for jerry script APIs
DEPS_BASE ?= $(ZJS_BASE)/deps
ZEPHYRINCLUDE += -I$(JERRY_INCLUDE) -I$(DEPS_BASE)
ZEPHYRINCLUDE += -I$(ZEPHYR_BASE)/samples/usb/webusb/src \
                 -I${ZEPHYR_BASE}/include/drivers/usb \
                 -I${ZEPHYR_BASE}/subsys/usb/class \
                 -I$(ZEPHYR_BASE)/include/usb
obj-y += ../../deps/zephyr/samples/usb/webusb/src/webusb_serial.o
obj-y += webusb.o
obj-y += comms-uart.o
obj-y += file-utils.o
obj-y += ashell.o
obj-y += ashell-ed.o
obj-y += ashell-cmd.o
obj-y += script-handler.o


# Select extended ANSI C/POSIX function set in recent Newlib versions
ccflags-y += -D_XOPEN_SOURCE=700

ccflags-y += -Wall -Werror
