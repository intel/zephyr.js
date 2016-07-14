ccflags-y += -I$(JERRY_BASE)/jerry-core
ccflags-y += -I$(ZEPHYR_BASE)/drivers
ccflags-y += -I$(ZJS_BASE)/outdir/include

ifeq ($(BOARD), qemu_x86)
ccflags-y += -DQEMU_BUILD
endif

obj-y += main.o \
         zjs_aio.o \
         zjs_ble.o \
         zjs_buffer.o \
         zjs_gpio.o \
         zjs_ipm.o \
         zjs_modules.o \
         zjs_pwm.o \
         zjs_script_gen.o \
         zjs_timers.o \
         zjs_util.o

obj-$(CONFIG_BOARD_ARDUINO_101) += zjs_a101_pins.o
