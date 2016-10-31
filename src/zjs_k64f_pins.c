// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include <zjs_gpio.h>
#include <zjs_pwm.h>
#include <zjs_util.h>

#ifdef BUILD_MODULE_GPIO
static void zjs_k64f_num_to_gpio(uint32_t num, int *dev, int *pin)
{
    // TODO: IMPLEMENT ME!
    zjs_default_convert_pin(num, dev, pin);
}
#endif

#ifdef BUILD_MODULE_PWM
static void zjs_k64f_num_to_pwm(uint32_t num, int *dev, int *pin)
{
    int devnum = (num & 0xe0) >> 5;
    if (devnum > 3) {
        PRINT("Warning: invalid PWM device number\n");
        devnum = 0;
    }

    // TODO: IMPLEMENT ME!
    zjs_default_convert_pin(num, dev, pin);
}
#endif

jerry_value_t zjs_k64f_init()
{
    // effects: returns an object with FRDM-K64F pin mappings
#ifdef BUILD_MODULE_GPIO
    zjs_gpio_convert_pin = zjs_k64f_num_to_gpio;
#endif
#ifdef BUILD_MODULE_PWM
    zjs_pwm_convert_pin = zjs_k64f_num_to_pwm;
#endif

    jerry_value_t obj = jerry_create_object();

    const int PTA = 0x00;
    const int PTB = 0x20;
    const int PTC = 0x40;
    const int PTD = 0x60;
    const int PTE = 0x80;

    // These are all the Arduino GPIOs
    zjs_obj_add_number(obj, PTC + 16, "D0");  // verified I/O
    zjs_obj_add_number(obj, PTC + 17, "D1");  // verified I/O
    zjs_obj_add_number(obj, PTB +  9, "D2");  // verified I/O
    zjs_obj_add_number(obj, PTA +  1, "D3");  // I/O if preserve jtag off
    zjs_obj_add_number(obj, PTB + 23, "D4");  // verified I/O
    zjs_obj_add_number(obj, PTA +  2, "D5");  // I/O if preserve jtag off
    zjs_obj_add_number(obj, PTC +  2, "D6");  // verified I/O
    zjs_obj_add_number(obj, PTC +  3, "D7");  // verified I/O
    zjs_obj_add_number(obj, PTC + 12, "D8");  // PTA0 for Rev <= D (ver. I/O)
    zjs_obj_add_number(obj, PTC +  4, "D9");  // verified I/O
    zjs_obj_add_number(obj, PTD +  0, "D10");  // verified I/O
    zjs_obj_add_number(obj, PTD +  2, "D11");  // verified I/O
    zjs_obj_add_number(obj, PTD +  3, "D12");  // verified I/O
    zjs_obj_add_number(obj, PTD +  1, "D13");  // verified I/O
    zjs_obj_add_number(obj, PTE + 25, "D14");  // works as input, not output
    zjs_obj_add_number(obj, PTE + 24, "D15");  // works as input, not output

    // These are for onboard RGB LED
    zjs_obj_add_number(obj, PTB + 22, "LEDR");  // verified
    zjs_obj_add_number(obj, PTE + 26, "LEDG");  // verified
    zjs_obj_add_number(obj, PTB + 21, "LEDB");  // verified

    // These are onboard switches SW2 and SW3 (SW1 is Reset)
    zjs_obj_add_number(obj, PTC +  6, "SW2");  // verified (press: falling edge)
    zjs_obj_add_number(obj, PTA +  4, "SW3");

    // TODO: More pins at https://developer.mbed.org/platforms/FRDM-K64F/

    // PWM pins
    zjs_obj_add_number(obj, PTA +  1, "PWM0");
    zjs_obj_add_number(obj, PTA +  2, "PWM1");
    zjs_obj_add_number(obj, PTC +  2, "PWM2");
    zjs_obj_add_number(obj, PTC +  3, "PWM3");
    zjs_obj_add_number(obj, PTC + 12, "PWM4");
    zjs_obj_add_number(obj, PTC +  4, "PWM5");
    zjs_obj_add_number(obj, PTD +  0, "PWM6");
    zjs_obj_add_number(obj, PTD +  2, "PWM7");
    zjs_obj_add_number(obj, PTD +  3, "PWM8");
    zjs_obj_add_number(obj, PTD +  1, "PWM9");

    // TODO: It appears that some other GPIO pins can be used as analog inputs
    //   too, from the X86 side. We haven't tried that.
    zjs_obj_add_number(obj, PTB +  2, "A0");
    zjs_obj_add_number(obj, PTB +  3, "A1");
    zjs_obj_add_number(obj, PTB + 10, "A2");
    zjs_obj_add_number(obj, PTB + 11, "A3");
    zjs_obj_add_number(obj, PTC + 11, "A4");
    zjs_obj_add_number(obj, PTC + 10, "A5");

    return obj;
}
