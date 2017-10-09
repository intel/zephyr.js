// Copyright (c) 2016-2017, Intel Corporation.

// ZJS includes
#include "zjs_common.h"
#include "zjs_gpio.h"
#include "zjs_pwm.h"
#include "zjs_util.h"

#ifdef BUILD_MODULE_PWM
static void zjs_a101_num_to_pwm(u32_t num, int *dev, int *pin)
{
    *dev = 0;
    if (num < 0) {
        *pin = -1;
        return;
    }
    if (num < 4) {
        // support directly giving channel number 0-3
        *pin = num;
        return;
    }
    if (num >= 0x20 && num <= 0x23) {
        // support PWM0 through PWM3
        *pin = num - 0x20;
        return;
    }

    switch (num) {
    case 17:  // IO3
        *pin = 0;
        break;
    case 15:  // IO5
        *pin = 1;
        break;
    case 12:  // IO6
        *pin = 2;
        break;
    case 13:  // IO9
        *pin = 3;
        break;

    default:
        *pin = -1;
    }
}
#endif

jerry_value_t zjs_a101_init()
{
    // effects: returns an object with Arduino 101 pin mappings
#ifdef BUILD_MODULE_PWM
    zjs_pwm_convert_pin = zjs_a101_num_to_pwm;
#endif

    jerry_value_t obj = zjs_create_object();

    // These are all the GPIOs that can be accessed as GPIOs by the X86 side.
    zjs_obj_add_number(obj, "IO2",  18);
    zjs_obj_add_number(obj, "IO3",  17);  // doesn't seem to work as output
    zjs_obj_add_number(obj, "IO4",  19);
    zjs_obj_add_number(obj, "IO5",  15);  // doesn't seem to work as output
    zjs_obj_add_number(obj, "IO7",  20);
    zjs_obj_add_number(obj, "IO8",  16);
    zjs_obj_add_number(obj, "IO10", 11);
    zjs_obj_add_number(obj, "IO11", 10);
    zjs_obj_add_number(obj, "IO12",  9);
    zjs_obj_add_number(obj, "IO13",  8);  // output also displayed on LED0

    // These are onboard LEDs
    zjs_obj_add_number(obj, "LED0", 26);  // active low, red fault LED
    zjs_obj_add_number(obj, "LED1", 12);  // active low
    // LED2 is unavailable when SPI is in use because that reconfigures pin
    //   IO13 which this is tied to. The filesystem uses SPI to talk to the
    //   flash chip, so when you use filesystem APIs or ashell (which always
    //   uses the filesystem) this LED will not work.
    zjs_obj_add_number(obj, "LED2",  8);

    // These cannot currently be used as GPIOs because they are controlled by
    // the ARC side and we don't have support for that. But they can be used as
    // PWMs.
    zjs_obj_add_number(obj, "IO6", 12);
    zjs_obj_add_number(obj, "IO9", 13);

    zjs_obj_add_number(obj, "PWM0", 0x20);
    zjs_obj_add_number(obj, "PWM1", 0x21);
    zjs_obj_add_number(obj, "PWM2", 0x22);
    zjs_obj_add_number(obj, "PWM3", 0x23);

    // TODO: It appears that some other GPIO pins can be used as analog inputs
    //   too, from the X86 side. We haven't tried that.
    zjs_obj_add_number(obj, "A0", 10);
    zjs_obj_add_number(obj, "A1", 11);
    zjs_obj_add_number(obj, "A2", 12);
    zjs_obj_add_number(obj, "A3", 13);
    zjs_obj_add_number(obj, "A4", 14);
    zjs_obj_add_number(obj, "A5",  9);

    return obj;
}

JERRYX_NATIVE_MODULE (arduino101_pins, zjs_a101_init)
