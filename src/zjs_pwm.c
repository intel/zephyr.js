// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_PWM
// Zephyr includes
#include <zephyr.h>
#include <pwm.h>
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_pwm.h"
#include "zjs_util.h"

static const char *ZJS_POLARITY_NORMAL = "normal";
static const char *ZJS_POLARITY_REVERSE = "reverse";

#ifdef CONFIG_BOARD_FRDM_K64F
#define PWM_DEV_COUNT 4
#else
#define PWM_DEV_COUNT 1
#endif

static struct device *zjs_pwm_dev[PWM_DEV_COUNT];

void (*zjs_pwm_convert_pin)(uint32_t orig, int *dev, int *pin) =
    zjs_default_convert_pin;

static void zjs_pwm_set(jerry_value_t obj, double period, double pulseWidth)
{
    uint32_t orig_chan;
    zjs_obj_get_uint32(obj, "channel", &orig_chan);

    int devnum, channel;
    zjs_pwm_convert_pin(orig_chan, &devnum, &channel);

    if (pulseWidth > period) {
        ERR_PRINT("zjs_pwm_set: pulseWidth was greater than period\n");
        pulseWidth = period;
    }

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_POLARITY_REVERSE)) {
            if (period > pulseWidth) {
                pulseWidth = 0;
            } else {
                pulseWidth = period - pulseWidth;
            }
        }
    }

    DBG_PRINT("Setting [uSec] channel=%u dev=%u, period=%lu, pulse=%lu\n", channel, devnum, (uint32_t)(period * 1000), (uint32_t)(pulseWidth * 1000));
    pwm_pin_set_usec(zjs_pwm_dev[devnum], channel, (uint32_t)(period * 1000), (uint32_t)(pulseWidth * 1000));
}

static void zjs_pwm_set_cycles(jerry_value_t obj, uint32_t periodHW, uint32_t pulseWidthHW)
{
    uint32_t orig_chan;
    zjs_obj_get_uint32(obj, "channel", &orig_chan);

    int devnum, channel;
    zjs_pwm_convert_pin(orig_chan, &devnum, &channel);

    if (pulseWidthHW > periodHW) {
        ERR_PRINT("zjs_pwm_set: pulseWidth was greater than period\n");
        pulseWidthHW = periodHW;
    }

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_POLARITY_REVERSE)) {
            if (periodHW > pulseWidthHW) {
                pulseWidthHW = 0;
            } else {
                pulseWidthHW = periodHW - pulseWidthHW;
            }
        }
    }

    DBG_PRINT("Setting [cycles] channel=%u dev=%u, period=%lu, pulse=%lu\n", channel, devnum, (uint32_t)periodHW, (uint32_t)pulseWidthHW);
    pwm_pin_set_cycles(zjs_pwm_dev[devnum], channel, periodHW, pulseWidthHW);
}

static void zjs_pwm_set_period_cycles(jerry_value_t obj, double periodHW)
{
    // requires: obj is a PWM pin object, period is in hardware cycles
    //  effects: sets the PWM pin to the given period, records the period
    //             in the object
    double pulseWidth;
    zjs_obj_get_double(obj, "pulseWidth", &pulseWidth);

    // update the JS object
    double period = periodHW / sys_clock_hw_cycles_per_sec * 1000;
    zjs_obj_add_number(obj, period, "period");

    double pulseWidthHW = pulseWidth * sys_clock_hw_cycles_per_sec / 1000;

    zjs_pwm_set_cycles(obj, periodHW, pulseWidthHW);
}

static jerry_value_t zjs_pwm_pin_set_period_cycles(const jerry_value_t function_obj,
                                                   const jerry_value_t this,
                                                   const jerry_value_t argv[],
                                                   const jerry_length_t argc)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the period in hardware cycles, dependent on the
    //             underlying hardware (31.25ns each for Arduino 101)
    //  effects: updates the period of this PWM pin, using the finest grain
    //             units provided by the platform, providing the widest range
    if (argc < 1 || !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_pwm_pin_set_period_cycles: invalid argument");

    double periodHW = jerry_get_number_value(argv[0]);

    zjs_pwm_set_period_cycles(this, periodHW);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_pwm_pin_set_period(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the period in milliseconds (float)
    //  effects: updates the period of this PWM pin, getting as close as
    //             possible to what is requested given hardware constraints
    if (argc < 1 || !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_pwm_pin_set_period: invalid argument");

    double pulseWidth;
    if (!zjs_obj_get_double(this, "pulseWidth", &pulseWidth)) {
        return zjs_error("zjs_pwm_pin_set_period: pulseWidth was not initialized");
    }

    double period = jerry_get_number_value(argv[0]);
    // store the period in the pwm object
    zjs_obj_add_number(this, period, "period");

    ZJS_PRINT("set_period(): period: %lu, pulse: %lu\n", (uint32_t)period, (uint32_t)pulseWidth);

    zjs_pwm_set(this, period, pulseWidth);

    return ZJS_UNDEFINED;
}

static void zjs_pwm_set_pulse_width_cycles(jerry_value_t obj,
                                           double pulseWidthHW)
{
    // requires: obj is a PWM pin object, pulseWidth is in hardware cycles
    //  effects: sets the PWM pin to the given pulse width, records the pulse
    //             width in the object
    double period;
    zjs_obj_get_double(obj, "period", &period);

    // update the JS object
    double pulseWidth = pulseWidthHW / sys_clock_hw_cycles_per_sec * 1000;
    zjs_obj_add_number(obj, pulseWidth, "pulseWidth");

    double periodHW = period * sys_clock_hw_cycles_per_sec / 1000;

    zjs_pwm_set_cycles(obj, periodHW, pulseWidthHW);
}

static jerry_value_t zjs_pwm_pin_set_pulse_width_cycles(const jerry_value_t function_obj,
                                                        const jerry_value_t this,
                                                        const jerry_value_t argv[],
                                                        const jerry_length_t argc)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the pulse width in hardware cycles, dependent on
    //             the underlying hardware (31.25ns each for Arduino 101)
    //  effects: updates the pulse width of this PWM pin
    if (argc < 1 || !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_pwm_pin_set_pulse_width_cycles: invalid argument");

    double pulseWidth = jerry_get_number_value(argv[0]);
    // store the pulseWidth in the pwm object
    zjs_obj_add_number(this, pulseWidth, "pulseWidth");

    zjs_pwm_set_pulse_width_cycles(this, pulseWidth);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_pwm_pin_set_pulse_width(const jerry_value_t function_obj,
                                                 const jerry_value_t this,
                                                 const jerry_value_t argv[],
                                                 const jerry_length_t argc)
{
    // requires: this is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the pulse width in milliseconds (float)
    //  effects: updates the pulse width of this PWM pin
    if (argc < 1 || !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_pwm_pin_set_pulse_width: invalid argument");

    double period;
    zjs_obj_get_double(this, "period", &period);

    double pulseWidth = jerry_get_number_value(argv[0]);
    // store the pulseWidth in the pwm object
    zjs_obj_add_number(this, pulseWidth, "pulseWidth");

    zjs_pwm_set(this, period, pulseWidth);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_pwm_open(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    // requires: arg 0 is an object with these members: channel (int), period in
    //             hardware cycles (defaults to 255), pulse width in hardware
    //             cycles (defaults to 0), polarity (defaults to "normal")
    //  effects: returns a new PWMPin object representing the given channel
    if (argc < 1 || !jerry_value_is_object(argv[0]))
        return zjs_error("zjs_pwm_open: invalid argument");

    // data input object
    jerry_value_t data = argv[0];

    uint32_t channel;
    if (!zjs_obj_get_uint32(data, "channel", &channel))
        return zjs_error("zjs_pwm_open: missing required field");

    int devnum, newchannel;
    zjs_pwm_convert_pin(channel, &devnum, &newchannel);
    if (newchannel == -1)
        return zjs_error("zjs_pwm_open: invalid channel");

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
        if (!strcmp(buffer, ZJS_POLARITY_REVERSE))
            polarity = ZJS_POLARITY_REVERSE;
    }

    // create the PWMPin object
    jerry_value_t pin_obj = jerry_create_object();
    zjs_obj_add_function(pin_obj, zjs_pwm_pin_set_period, "setPeriod");
    zjs_obj_add_function(pin_obj, zjs_pwm_pin_set_period_cycles,
                         "setPeriodCycles");
    zjs_obj_add_function(pin_obj, zjs_pwm_pin_set_pulse_width, "setPulseWidth");
    zjs_obj_add_function(pin_obj, zjs_pwm_pin_set_pulse_width_cycles,
                         "setPulseWidthCycles");
    zjs_obj_add_number(pin_obj, channel, "channel");
    zjs_obj_add_number(pin_obj, period, "period");
    zjs_obj_add_number(pin_obj, pulseWidth, "pulseWidth");
    zjs_obj_add_string(pin_obj, polarity, "polarity");

    zjs_pwm_set(pin_obj, period, pulseWidth);

    // TODO: When we implement close, we should release the reference on this
    return pin_obj;
}

jerry_value_t zjs_pwm_init()
{
    // effects: finds the PWM driver and registers the PWM JS object
    char devname[10];

    for (int i = 0; i < PWM_DEV_COUNT; i++) {
        snprintf(devname, 7, "PWM_%d", i);
        zjs_pwm_dev[i] = device_get_binding(devname);
        if (!zjs_pwm_dev[i]) {
            ERR_PRINT("DEVICE: '%s'\n", devname);
            return zjs_error("zjs_pwm_init: cannot find PWM device");
        }
    }

    // create PWM object
    jerry_value_t pwm_obj = jerry_create_object();
    zjs_obj_add_function(pwm_obj, zjs_pwm_open, "open");
    return pwm_obj;
}
#endif // BUILD_MODULE_PWM
