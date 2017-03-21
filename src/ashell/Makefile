# Copyright © 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifdef V
$(info Compiling application)
endif

# Adding path for jerry script APIs
DEPS_BASE ?= $(ZJS_BASE)/deps
ZEPHYRINCLUDE += -I$(JERRY_INCLUDE) -I$(DEPS_BASE)
ifneq ($(CONFIG_USB_CDC_ACM),y)
ZEPHYRINCLUDE += -I$(ZEPHYR_BASE)/samples/usb/webusb/src \
                 -I${ZEPHYR_BASE}/include/drivers/usb \
                 -I${ZEPHYR_BASE}/subsys/usb/class \
                 -I$(ZEPHYR_BASE)/include/usb
obj-y += ../../deps/zephyr/samples/usb/webusb/src/webusb_serial.o
obj-y += webusb-handler.o
endif

obj-y += file-utils.o
obj-y += jerry-code.o

obj-y += ihex-handler.o

obj-y += comms-uart.o
obj-y += comms-shell.o
obj-y += shell-state.o

obj-y += ../../deps/ihex/kk_ihex_read.o

ccflags-y += -Wall -Werror
