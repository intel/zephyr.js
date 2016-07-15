// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include <zjs_gpio.h>
#include <zjs_pwm.h>
#include <zjs_util.h>

static int zjs_a101_num_to_gpio(int num)
{
    if ((num >= 0 && num <= 3) || (num >= 15 && num <= 20) ||
        num == 8 || num == 12)
        return num;
    // not supported, at least currently
    return -1;
}

static int zjs_a101_num_to_pwm(int num)
{
    if (num < 0)
        return -1;
    if (num < 4)
        // support directly giving channel number 0-3
        return num;
    if (num >= 0x20 && num <= 0x23)
        // support PWM0 through PWM3
        return num - 0x20;
    
    switch (num) {
    case 17:  // IO3
        return 0;
    case 15:  // IO5
        return 1;
    case 12:  // IO6
        return 2;
    case 13:  // IO9
        return 3;

    default:
        return -1;
    }
}

jerry_object_t *zjs_a101_init()
{
    // effects: returns an object with Arduino 101 pin mappings
    zjs_gpio_convert_pin = zjs_a101_num_to_gpio;
    zjs_pwm_convert_pin = zjs_a101_num_to_pwm;

    jerry_object_t *obj = jerry_create_object();

    // These are all the GPIOs that can be accessed as GPIOs by the X86 side.
    zjs_obj_add_number(obj, 18, "IO2");
    zjs_obj_add_number(obj, 17, "IO3");  // doesn't seem to work as output
    zjs_obj_add_number(obj, 19, "IO4");
    zjs_obj_add_number(obj, 15, "IO5");  // doesn't seem to work as output
    zjs_obj_add_number(obj, 20, "IO7");
    zjs_obj_add_number(obj, 16, "IO8");
    zjs_obj_add_number(obj, 0,  "IO10");
    zjs_obj_add_number(obj, 3,  "IO11");
    zjs_obj_add_number(obj, 1,  "IO12");
    zjs_obj_add_number(obj, 2,  "IO13");

    // These are two onboard LEDs
    zjs_obj_add_number(obj, 8,  "LED0");
    zjs_obj_add_number(obj, 12, "LED1");  // active low

    // These cannot currently be used as GPIOs because they are controlled by
    // the ARC side and we don't have support for that. But they can be used as
    // PWMs.
    zjs_obj_add_number(obj, 12, "IO6");
    zjs_obj_add_number(obj, 13, "IO9");

    zjs_obj_add_number(obj, 0x20,  "PWM0");
    zjs_obj_add_number(obj, 0x21,  "PWM1");
    zjs_obj_add_number(obj, 0x22,  "PWM2");
    zjs_obj_add_number(obj, 0x23,  "PWM3");

    // TODO: It appears that some other GPIO pins can be used as analog inputs
    //   too, from the X86 side. We haven't tried that.
    zjs_obj_add_number(obj, 10, "A0");
    zjs_obj_add_number(obj, 11, "A1");
    zjs_obj_add_number(obj, 12, "A2");
    zjs_obj_add_number(obj, 13, "A3");
    zjs_obj_add_number(obj, 14, "A4");
    zjs_obj_add_number(obj, 9,  "A5");

    return obj;
}
