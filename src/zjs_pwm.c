// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <pwm.h>
#include <misc/util.h>
#include <string.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

// ZJS includes
#include "zjs_pwm.h"
#include "zjs_util.h"

static const char *ZJS_POLARITY_NORMAL = "normal";
static const char *ZJS_POLARITY_INVERSE = "inversed";

static struct device *zjs_pwm_dev;

jerry_api_object_t *zjs_pwm_init()
{
    // effects: finds the PWM driver and registers the PWM JS object
    zjs_pwm_dev = device_get_binding("PWM_0");
    if (!zjs_pwm_dev) {
        PRINT("error: cannot find PWM_0 device\n");
    }

    // create PWM object
    jerry_api_object_t *pwm_obj = jerry_api_create_object();
    zjs_obj_add_function(pwm_obj, zjs_pwm_open, "open");
    return pwm_obj;
}

bool zjs_pwm_open(const jerry_api_object_t *function_obj_p,
                  const jerry_api_value_t *this_p,
                  jerry_api_value_t *ret_val_p,
                  const jerry_api_value_t args_p[],
                  const jerry_api_length_t args_cnt)
{
    // requires: arg 0 is an object with these members: pin (int), period,
    //             duty cycle, polarity (defaults to "normal"
    if (args_cnt < 1 ||
        args_p[0].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_pwm_open: invalid argument\n");
        return false;
    }

    // data input object
    jerry_api_object_t *data = args_p[0].u.v_object;

    uint32_t required[3];
    const char *names[3] = { "channel", "period", "dutyCycle" };

    for (int i=0; i<3; i++) {
        if (!zjs_obj_get_uint32(data, names[i], &required[i])) {
            PRINT("zjs_pwm_open: missing required field\n");
            return false;
        }
    }

    uint32_t channel   = required[0];
    uint32_t period    = required[1];
    uint32_t dutyCycle = required[2];

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    const char *polarity = ZJS_POLARITY_NORMAL;
    if (zjs_obj_get_string(data, "polarity", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_POLARITY_INVERSE))
            polarity = ZJS_POLARITY_INVERSE;
    }

    // set the inital timing
    zjs_pwm_set(channel, period, dutyCycle, polarity);

    // create the PWMPin object
    jerry_api_object_t *pinobj = jerry_api_create_object();
    zjs_obj_add_function(pinobj, zjs_pwm_pin_set_period, "setPeriod");
    zjs_obj_add_function(pinobj, zjs_pwm_pin_set_duty_cycle, "setDutyCycle");
    zjs_obj_add_uint32(pinobj, required[0], "channel");
    zjs_obj_add_uint32(pinobj, required[1], "period");
    zjs_obj_add_uint32(pinobj, required[2], "dutyCycle");
    zjs_obj_add_string(pinobj, polarity, "polarity");
    // TODO: When we implement close, we should release the reference on this

    *ret_val_p = jerry_api_create_object_value(pinobj);
    return true;
}

bool zjs_pwm_pin_set_period(const jerry_api_object_t *function_obj_p,
                            const jerry_api_value_t *this_p,
                            jerry_api_value_t *ret_val_p,
                            const jerry_api_value_t args_p[],
                            const jerry_api_length_t args_cnt)
{
    // requires: this_p is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the period in hardware cycles, dependent on the
    //             underlying hardware (31.25ns each for Arduino 101)
    //  effects: updates the period of this PWM pin
    jerry_api_object_t *obj = jerry_api_get_object_value(this_p);

    if (args_cnt < 1 ||
        (args_p[0].type != JERRY_API_DATA_TYPE_UINT32 &&
         args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32 &&
         args_p[0].type != JERRY_API_DATA_TYPE_FLOAT64))
    {
        PRINT("zjs_pwm_pin_set_period: invalid argument\n");
        return false;
    }

    uint32_t period = (uint32_t)jerry_api_get_number_value(&args_p[0]);

    uint32_t channel, dutyCycle;
    zjs_obj_get_uint32(obj, "channel", &channel);
    zjs_obj_get_uint32(obj, "dutyCycle", &dutyCycle);

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    const char *polarity = ZJS_POLARITY_NORMAL;
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_POLARITY_INVERSE))
            polarity = ZJS_POLARITY_INVERSE;
    }

    // update the JS object
    zjs_obj_add_uint32(obj, period, "period");

    zjs_pwm_set(channel, period, dutyCycle, polarity);
    return true;
}

bool zjs_pwm_pin_set_duty_cycle(const jerry_api_object_t *function_obj_p,
                                const jerry_api_value_t *this_p,
                                jerry_api_value_t *ret_val_p,
                                const jerry_api_value_t args_p[],
                                const jerry_api_length_t args_cnt)
{
    // requires: this_p is a PWMPin object from zjs_pwm_open, takes one
    //             argument, the duty cycle in microseconds
    //  effects: updates the duty cycle of this PWM pin
    jerry_api_object_t *obj = jerry_api_get_object_value(this_p);

    if (args_cnt < 1 ||
        (args_p[0].type != JERRY_API_DATA_TYPE_UINT32 &&
         args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32 &&
         args_p[0].type != JERRY_API_DATA_TYPE_FLOAT64))
    {
        PRINT("zjs_pwm_pin_set_duty_cycle: invalid argument\n");
        return false;
    }

    uint32_t dutyCycle = (uint32_t)jerry_api_get_number_value(&args_p[0]);

    uint32_t channel, period;
    zjs_obj_get_uint32(obj, "channel", &channel);
    zjs_obj_get_uint32(obj, "period", &period);

    const int BUFLEN = 10;
    char buffer[BUFLEN];
    const char *polarity = ZJS_POLARITY_NORMAL;
    if (zjs_obj_get_string(obj, "polarity", buffer, BUFLEN)) {
        if (!strcmp(buffer, ZJS_POLARITY_INVERSE))
            polarity = ZJS_POLARITY_INVERSE;
    }

    zjs_pwm_set(channel, period, dutyCycle, polarity);
    return true;
}

void zjs_pwm_set(uint32_t channel, uint32_t period, uint32_t dutyCycle,
                 const char *polarity)
{
    // requires: channel is 0-3 on Arduino 101, period is the time in
    //             microseconds for the on/off cycle to complete, duty cycle
    //             is the time in microseconds for the signal to be on,
    //             polarity is "normal" if on means high, "inversed" if on
    //             means low
    //  effects: sets the given period and duty cycle, but the true duty
    //            cycle will always be on and off for at least one hw cycle

    // convert from ns to hw cycles
    uint32_t periodHW = (uint64_t)period * sys_clock_hw_cycles_per_sec /
        (uint32_t)1E9;
    uint32_t dutyHW = (uint64_t)dutyCycle * sys_clock_hw_cycles_per_sec /
        (uint32_t)1E9;
    if (periodHW < 2) {
        // period must be at least two cycles
        periodHW = 2;
    }

    // workaround the fact that Zephyr API won't allow fully on or off
    if (dutyHW == 0) {
        // must be on for at least one cycle
        dutyHW = 1;
    }
    if (dutyHW >= periodHW) {
        // must be off for at least one cycle
        dutyHW = periodHW - 1;
    }

    uint32_t offdutyHW = periodHW - dutyHW;

    uint32_t onTime, offTime;
    if (strcmp(polarity, ZJS_POLARITY_INVERSE)) {
        onTime = dutyHW;
        offTime = offdutyHW;
    }
    else {
        onTime = offdutyHW;
        offTime = dutyHW;
    }

    pwm_pin_set_values(zjs_pwm_dev, channel, onTime, offTime);
}
