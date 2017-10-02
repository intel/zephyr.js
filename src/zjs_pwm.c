// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <misc/util.h>
#include <pwm.h>
#include <zephyr.h>

// JerryScript includes
#include "jerryscript-ext/module.h"

// ZJS includes
#include "zjs_pwm.h"
#include "zjs_util.h"

static jerry_value_t zjs_pwm_pin_prototype;

static const char *ZJS_POLARITY_NORMAL = "normal";
static const char *ZJS_POLARITY_REVERSE = "reverse";

#ifdef CONFIG_BOARD_FRDM_K64F
#define PWM_DEV_COUNT 4
#else
#define PWM_DEV_COUNT 1
#endif

static struct device *zjs_pwm_dev[PWM_DEV_COUNT];

void (*zjs_pwm_convert_pin)(u32_t orig,
                            int *dev,
                            int *pin) = zjs_default_convert_pin;

static void zjs_pwm_set_cycles(jerry_value_t obj, u32_t periodHW,
                               u32_t pulseWidthHW)
{
    u32_t orig_chan;
    zjs_obj_get_uint32(obj, "channel", &orig_chan);

    int devnum, channel;
    zjs_pwm_convert_pin(orig_chan, &devnum, &channel);

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strncmp(buffer, ZJS_POLARITY_REVERSE, BUFLEN)) {
            pulseWidthHW = periodHW - pulseWidthHW;
        }
    }

    DBG_PRINT("Setting [cycles] channel=%d dev=%d, period=%lu, pulse=%lu\n",
              channel, devnum, (u32_t)periodHW, (u32_t)pulseWidthHW);
    pwm_pin_set_cycles(zjs_pwm_dev[devnum], channel, periodHW, pulseWidthHW);
}

static void zjs_pwm_set_ms(jerry_value_t obj, double period, double pulseWidth)
{
    u32_t orig_chan;
    zjs_obj_get_uint32(obj, "channel", &orig_chan);

    int devnum, channel;
    zjs_pwm_convert_pin(orig_chan, &devnum, &channel);

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strncmp(buffer, ZJS_POLARITY_REVERSE, BUFLEN)) {
            pulseWidth = period - pulseWidth;
        }
    }

    DBG_PRINT("Setting [uSec] channel=%d dev=%d, period=%lu, pulse=%lu\n",
              channel, devnum, (u32_t)(period * 1000),
              (u32_t)(pulseWidth * 1000));
    pwm_pin_set_usec(zjs_pwm_dev[devnum], channel, (u32_t)(period * 1000),
                     (u32_t)(pulseWidth * 1000));
}

static ZJS_DECL_FUNC(zjs_pwm_pin_set_cycles)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes two arguments:
    //             the period in hardware cycles, dependent on the underlying
    //             hardware (31.25ns each for Arduino 101), and the pulse width
    //             in hardware cycles
    //  effects: updates the period and pulse width of this PWM pin, using the
    //             finest grain units provided by the platform, providing the
    //             widest range

    // args: period in cycles, pulse width in cycles
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    double periodHW = jerry_get_number_value(argv[0]);
    double pulseWidthHW = jerry_get_number_value(argv[1]);

    if (pulseWidthHW > periodHW) {
        DBG_PRINT("pulseWidth was greater than period\n");
        return zjs_error("pulseWidth must not be greater than period");
    }

    // update the JS object
    double period = periodHW / sys_clock_hw_cycles_per_sec * 1000;
    double pulseWidth = pulseWidthHW / sys_clock_hw_cycles_per_sec * 1000;
    zjs_obj_add_number(this, "period", period);
    zjs_obj_add_number(this, "pulseWidth", pulseWidth);

    zjs_pwm_set_cycles(this, periodHW, pulseWidthHW);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pwm_pin_set_ms)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes two arguments:
    //             the period in milliseconds (float), and the pulse width in
    //             milliseconds (float)
    //  effects: updates the period of this PWM pin, getting as close as
    //             possible to what is requested given hardware constraints

    // args: period in milliseconds, pulse width in milliseconds
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    double period = jerry_get_number_value(argv[0]);
    double pulseWidth = jerry_get_number_value(argv[1]);

    if (pulseWidth > period) {
        DBG_PRINT("pulseWidth was greater than period\n");
        return zjs_error("pulseWidth must not be greater than period");
    }

    // store the values in the pwm object
    zjs_obj_add_number(this, "period", period);
    zjs_obj_add_number(this, "pulseWidth", period);

    DBG_PRINT("period: %lu, pulse: %lu\n", (u32_t)period, (u32_t)pulseWidth);

    zjs_pwm_set_ms(this, period, pulseWidth);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pwm_open)
{
    // requires: arg 0 is an object with these members: channel (int), period in
    //             hardware cycles (defaults to 255), pulse width in hardware
    //             cycles (defaults to 0), polarity (defaults to "normal")
    //  effects: returns a new PWMPin object representing the given channel

    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    // data input object
    jerry_value_t data = argv[0];

    u32_t channel;
    if (!zjs_obj_get_uint32(data, "channel", &channel))
        return zjs_error("missing required field");

    int devnum, newchannel;
    zjs_pwm_convert_pin(channel, &devnum, &newchannel);
    if (newchannel == -1)
        return zjs_error("invalid channel");

    double period, pulseWidth;
    if (!zjs_obj_get_double(data, "period", &period)) {
        period = 0;
    }
    if (!zjs_obj_get_double(data, "pulseWidth", &pulseWidth)) {
        pulseWidth = 0;
    }

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    const char *polarity = ZJS_POLARITY_NORMAL;
    if (zjs_obj_get_string(data, "polarity", buffer, BUFLEN)) {
        if (strequal(buffer, ZJS_POLARITY_REVERSE))
            polarity = ZJS_POLARITY_REVERSE;
    }

    // create the PWMPin object
    jerry_value_t pin_obj = zjs_create_object();
    jerry_set_prototype(pin_obj, zjs_pwm_pin_prototype);

    zjs_obj_add_number(pin_obj, "channel", channel);
    zjs_obj_add_number(pin_obj, "period", period);
    zjs_obj_add_number(pin_obj, "pulseWidth", pulseWidth);
    zjs_obj_add_string(pin_obj, "polarity", polarity);

    zjs_pwm_set_ms(pin_obj, period, pulseWidth);

    // TODO: When we implement close, we should release the reference on this
    return pin_obj;
}

static void zjs_pwm_cleanup(void *data)
{
    jerry_release_value(*(jerry_value_t *)data);
}

static const jerry_object_native_info_t cleanup_info =
{
    .free_cb = zjs_pwm_cleanup
};

static jerry_value_t zjs_pwm_init()
{
    // effects: finds the PWM driver and registers the PWM JS object
    char devname[10];

    for (int i = 0; i < PWM_DEV_COUNT; i++) {
        snprintf(devname, 7, "PWM_%d", i);
        zjs_pwm_dev[i] = device_get_binding(devname);
        if (!zjs_pwm_dev[i]) {
            ERR_PRINT("DEVICE: '%s'\n", devname);
            return zjs_error_context("cannot find PWM device", 0, 0);
        }
    }

    // create PWM pin prototype object
    zjs_native_func_t array[] = {
        { zjs_pwm_pin_set_ms, "setMilliseconds" },
        { zjs_pwm_pin_set_cycles, "setCycles" },
        { NULL, NULL }
    };
    zjs_pwm_pin_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_pwm_pin_prototype, array);

    // create PWM object
    jerry_value_t pwm_obj = zjs_create_object();
    zjs_obj_add_function(pwm_obj, "open", zjs_pwm_open);

    jerry_set_object_native_pointer(pwm_obj, &zjs_pwm_pin_prototype,
                                    &cleanup_info);

    return pwm_obj;
}

JERRYX_NATIVE_MODULE(pwm, zjs_pwm_init)
