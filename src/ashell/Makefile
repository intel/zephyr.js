# Copyright (c) 2016-2017, Intel Corporation.

ifdef V
$(info Compiling application)
endif

# Adding path for jerry script APIs
DEPS_BASE ?= $(ZJS_BASE)/deps
ZEPHYRINCLUDE += -I$(JERRY_INCLUDE) -I$(DEPS_BASE)
ifneq ($(CONFIG_USB_CDC_ACM),y)
ZEPHYRINCLUDE += -I$(ZEPHYR_BASE)/samples/subsys/usb/webusb/src \
                 -I${ZEPHYR_BASE}/include/drivers/usb \
                 -I${ZEPHYR_BASE}/subsys/usb/class \
                 -I$(ZEPHYR_BASE)/include/usb
obj-y += ../../deps/zephyr/samples/subsys/usb/webusb/src/webusb_serial.o
obj-y += term-webusb.o
endif

obj-y += file-utils.o
obj-y += jerry-code.o

obj-y += term-ihex.o

obj-y += term-uart.o
obj-y += term-cmd.o

obj-y += ../../deps/ihex/kk_ihex_read.o

ifneq ($(BOARD), linux)
# Select extended ANSI C/POSIX function set in recent Newlib versions
ccflags-y += -D_XOPEN_SOURCE=700
endif

ccflags-y += -DZJS_ASHELL -Wall -Werror
