// Copyright (c) 2016-2017, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include "zjs_pwm.h"
#include "zjs_util.h"

#ifdef BUILD_MODULE_PWM
static void zjs_k64f_num_to_pwm(u32_t num, int *dev, int *pin)
{
    int devnum = (num & 0xe0) >> 5;
    if (devnum > 3) {
        DBG_PRINT("Warning: invalid PWM device number\n");
        devnum = 0;
    }

    // TODO: IMPLEMENT ME!
    zjs_default_convert_pin(num, dev, pin);
}
#endif

jerry_value_t zjs_k64f_init()
{
    // effects: returns an object with FRDM-K64F pin mappings
#ifdef BUILD_MODULE_PWM
    zjs_pwm_convert_pin = zjs_k64f_num_to_pwm;
#endif

    jerry_value_t obj = zjs_create_object();

    const int PTA = 0x00;
    const int PTB = 0x20;
    const int PTC = 0x40;
    const int PTD = 0x60;
    const int PTE = 0x80;

    // These are all the Arduino GPIOs
    zjs_obj_add_number(obj, "D0", PTC + 16);  // verified I/O
    zjs_obj_add_number(obj, "D1", PTC + 17);  // verified I/O
    zjs_obj_add_number(obj, "D2", PTB +  9);  // verified I/O
    zjs_obj_add_number(obj, "D3", PTA +  1);  // verified I/O
    zjs_obj_add_number(obj, "D4", PTB + 23);  // verified I/O
    zjs_obj_add_number(obj, "D5", PTA +  2);  // verified I/O
    zjs_obj_add_number(obj, "D6", PTC +  2);  // verified I/O
    zjs_obj_add_number(obj, "D7", PTC +  3);  // verified I/O

    // currently not working on rev E3; used to work as input/output
    zjs_obj_add_number(obj, "D8", PTC + 12);  // PTA0 for Rev <= D

    zjs_obj_add_number(obj, "D9",  PTC +  4);  // verified I/O
    zjs_obj_add_number(obj, "D10", PTD +  0);  // verified I/O
    zjs_obj_add_number(obj, "D11", PTD +  2);  // verified I/O
    zjs_obj_add_number(obj, "D12", PTD +  3);  // verified I/O
    zjs_obj_add_number(obj, "D13", PTD +  1);  // verified I/O
    zjs_obj_add_number(obj, "D14", PTE + 25);  // verified I/O
    zjs_obj_add_number(obj, "D15", PTE + 24);  // verified I/O

    // These are for onboard RGB LED
    zjs_obj_add_number(obj, "LEDR", PTB + 22);  // verified
    zjs_obj_add_number(obj, "LEDG", PTE + 26);  // verified
    zjs_obj_add_number(obj, "LEDB", PTB + 21);  // verified

    // These are onboard switches SW2 and SW3 (SW1 is Reset)
    zjs_obj_add_number(obj, "SW2", PTC +  6);  // verified (press: falling edge)
    zjs_obj_add_number(obj, "SW3", PTA +  4);

    // TODO: More pins at https://developer.mbed.org/platforms/FRDM-K64F/

    // PWM pins
    zjs_obj_add_number(obj, "PWM0", PTA +  1);
    zjs_obj_add_number(obj, "PWM1", PTA +  2);
    zjs_obj_add_number(obj, "PWM2", PTC +  2);
    zjs_obj_add_number(obj, "PWM3", PTC +  3);
    zjs_obj_add_number(obj, "PWM4", PTC + 12);
    zjs_obj_add_number(obj, "PWM5", PTC +  4);
    zjs_obj_add_number(obj, "PWM6", PTD +  0);
    zjs_obj_add_number(obj, "PWM7", PTD +  2);
    zjs_obj_add_number(obj, "PWM8", PTD +  3);
    zjs_obj_add_number(obj, "PWM9", PTD +  1);

    // TODO: It appears that some other GPIO pins can be used as analog inputs
    //   too, from the X86 side. We haven't tried that.
    zjs_obj_add_number(obj, "A0", PTB +  2);
    zjs_obj_add_number(obj, "A1", PTB +  3);
    zjs_obj_add_number(obj, "A2", PTB + 10);
    zjs_obj_add_number(obj, "A3", PTB + 11);
    zjs_obj_add_number(obj, "A4", PTC + 11);
    zjs_obj_add_number(obj, "A5", PTC + 10);

    return obj;
}

JERRYX_NATIVE_MODULE (k64f_pins, zjs_k64f_init)
