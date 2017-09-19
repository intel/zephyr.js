# Copyright (c) 2016-2017, Intel Corporation.

JERRY_BASE ?= $(ZJS_BASE)/deps/jerryscript
OCF_ROOT ?= deps/iotivity-constrained

ccflags-y += -Wall -Werror $(ZJS_FLAGS)

# TODO: iotivity-constrained gives some warnings and until we figure out how to
#       disable them just for iotivity-constrained, we need to turn them back
#       into warnings
ccflags-y += -Wno-error=format

# Select extended ANSI C/POSIX function set in recent Newlib versions
ccflags-y += -D_XOPEN_SOURCE=700

ccflags-y += -I$(JERRY_BASE)/jerry-core/include
ccflags-y += -I$(JERRY_BASE)/jerry-core/jrt
ccflags-y += -I$(ZEPHYR_BASE)/drivers
ccflags-y += -I$(ZJS_BASE)/outdir/include

ifeq ($(VARIANT), debug)
ccflags-y += -DDEBUG_BUILD
ccflags-y += -g
endif

ifeq ($(BOARD), qemu_x86)
ccflags-y += -DQEMU_BUILD
endif

ifeq ($(CB_STATS), on)
ccflags-y += -DZJS_PRINT_CALLBACK_STATS
endif

ifeq ($(PRINT_FLOAT), on)
export LDFLAGS += -u _printf_float
ccflags-y += -DZJS_PRINT_FLOATS
endif

ifneq ($(BLE_ADDR), none)
ccflags-y += -DZJS_CONFIG_BLE_ADDRESS=\"$(BLE_ADDR)\"
endif

obj-y += main.o \
         zjs_callbacks.o \
         zjs_common.o \
         zjs_error.o \
         zjs_modules.o \
         zjs_script.o \
         zjs_timers.o \
         zjs_util.o \
         jerry-port/zjs_jerry_port.o

ifneq (,$(findstring zjs_ashell,$(ASHELL)))
$(info Insecure Mode (development))
export JERRY_INCLUDE = $(JERRY_BASE)/jerry-core/include
endif
obj-y += \
	zjs_ipm.o \
	zjs_aio.o \
	zjs_a101_pins.o \
	zjs_event.o \
	zjs_buffer.o \
	zjs_ble.o \
	zjs_console.o \
	zjs_fs.o \
	zjs_board.o \
	zjs_gpio.o \
	zjs_gpio_mock.o \
	zjs_grove_lcd_ipm.o \
	zjs_i2c_ipm_handler.o \
	zjs_i2c.o \
	zjs_performance.o \
	zjs_pwm.o \
	zjs_sensor_board_a101.o \
	zjs_sensor.o \
	sensors/zjs_sensor_accel.o \
	sensors/zjs_sensor_gyro.o \
	sensors/zjs_sensor_light.o \
	sensors/zjs_sensor_temp.o \
	zjs_spi.o \
	ashell/ \

ccflags-y += \
	-DBUILD_MODULE_AIO \
	-DBUILD_MODULE_A101 \
	-DBUILD_MODULE_EVENT \
	-DBUILD_MODULE_BUFFER \
	-DBUILD_MODULE_BLE \
	-DBUILD_MODULE_CONSOLE \
	-DBUILD_MODULE_FS \
	-DBUILD_MODULE_BOARD \
	-DBUILD_MODULE_GPIO \
	-DBUILD_MODULE_GROVE_LCD \
	-DBUILD_MODULE_I2C \
	-DBUILD_MODULE_PERFORMANCE \
	-DBUILD_MODULE_PWM \
	-DBUILD_MODULE_SENSOR \
	-I$(ZJS_BASE)/src/sensors \
	-DBUILD_MODULE_SENSOR_ACCEL \
	-DBUILD_MODULE_SENSOR_GYRO \
	-DBUILD_MODULE_SENSOR_LIGHT \
	-DBUILD_MODULE_SENSOR_TEMP \
	-DBUILD_MODULE_SPI \
	-DZJS_ASHELL \

