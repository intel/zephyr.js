// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_PWM

// C includes
#include <string.h>

// Zephyr includes
#include <misc/util.h>
#include <pwm.h>
#include <zephyr.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_pwm.h"
#include "zjs_util.h"

static jerry_value_t zjs_pwm_pin_prototype;

typedef struct {
    struct device *device;
    u32_t pin;
    jerry_value_t pin_obj;
    bool reverse;
} pwm_handle_t;

static void zjs_pwm_free_cb(void *native)
{
    pwm_handle_t *handle = (pwm_handle_t *)native;
    // set to pulse of 0 w/ arbitrary period of 100 to turn it off
    pwm_pin_set_cycles(handle->device, handle->pin, 0, 100);
    zjs_free(handle);
}

static const jerry_object_native_info_t pwm_type_info = {
    .free_cb = zjs_pwm_free_cb
};

#ifdef CONFIG_BOARD_FRDM_K64F
#define PWM_DEV_COUNT 4
#else
#define PWM_DEV_COUNT 1
#endif

static struct device *zjs_pwm_dev[PWM_DEV_COUNT];

void (*zjs_pwm_convert_pin)(u32_t orig, int *dev,
                            int *pin) = zjs_default_convert_pin;

static void zjs_pwm_set_cycles(jerry_value_t obj, u32_t periodHW,
                               u32_t pulseWidthHW)
{
    ZJS_GET_HANDLE_OR_NULL(obj, pwm_handle_t, handle, pwm_type_info);
    if (!handle) {
        DBG_PRINT("obj %p\n", (void *)obj);
        ZJS_ASSERT(false, "no handle found");
        return;
    }

    if (handle->reverse) {
        pulseWidthHW = periodHW - pulseWidthHW;
    }

    DBG_PRINT("Setting [cycles] dev=%p, pin=%d period=%u, pulse=%u\n",
              handle->device, handle->pin, (u32_t)periodHW,
              (u32_t)pulseWidthHW);
    pwm_pin_set_cycles(handle->device, handle->pin, periodHW, pulseWidthHW);
}

static void zjs_pwm_set_ms(jerry_value_t obj, double period, double pulseWidth)
{
    ZJS_GET_HANDLE_OR_NULL(obj, pwm_handle_t, handle, pwm_type_info);
    if (!handle) {
        ZJS_ASSERT(false, "no handle found");
        return;
    }

    if (handle->reverse) {
        pulseWidth = period - pulseWidth;
    }

    DBG_PRINT("Setting [uSec] dev=%p, pin=%d period=%u, pulse=%u\n",
              handle->device, handle->pin, (u32_t)(period * 1000),
              (u32_t)(pulseWidth * 1000));
    pwm_pin_set_usec(handle->device, handle->pin, (u32_t)(period * 1000),
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

    DBG_PRINT("period: %u, pulse: %u\n", (u32_t)period, (u32_t)pulseWidth);

    zjs_pwm_set_ms(this, period, pulseWidth);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_pwm_open)
{
    // requires: arg 0 is a pin number or string, or an object with these
    //             members: pin (int or string), reversePolarity (defaults to
    //             false)
    //  effects: returns a new PWMPin object representing the given channel

    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_NUMBER Z_STRING Z_OBJECT);

    ZVAL_MUTABLE pin_str = 0;
    jerry_value_t pin_val = argv[0];
    jerry_value_t init = 0;

    if (jerry_value_is_object(argv[0])) {
        init = argv[0];
        pin_str = zjs_get_property(init, "pin");
        pin_val = pin_str;
    }

    char devname[20];
    int pin = zjs_board_find_pwm(pin_val, devname, 20);
    if (pin == FIND_PIN_INVALID) {
        return TYPE_ERROR("bad pin argument");
    } else if (pin == FIND_DEVICE_FAILURE) {
        return zjs_error("device not found");
    } else if (pin < 0) {
        return zjs_error("pin not found");
    }
    struct device *pwmdev = device_get_binding(devname);

    // set defaults
    bool reverse = false;

    if (init) {
        zjs_obj_get_boolean(init, "reversePolarity", &reverse);
    }

    // create the PWMPin object
    ZVAL pin_obj = zjs_create_object();
    jerry_set_prototype(pin_obj, zjs_pwm_pin_prototype);

    pwm_handle_t *handle = zjs_malloc(sizeof(pwm_handle_t));
    if (!handle) {
        return zjs_error("out of memory");
    }

    memset(handle, 0, sizeof(pwm_handle_t));
    handle->device = pwmdev;
    handle->pin = pin;
    handle->reverse = reverse;
    handle->pin_obj = pin_obj;  // weak reference

    jerry_set_object_native_pointer(pin_obj, handle, &pwm_type_info);

    return jerry_acquire_value(pin_obj);
}

static void zjs_pwm_cleanup(void *native)
{
    jerry_release_value(zjs_pwm_pin_prototype);
}

static const jerry_object_native_info_t pwm_module_type_info = {
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
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(pwm_obj, NULL, &pwm_module_type_info);
    return pwm_obj;
}

JERRYX_NATIVE_MODULE(pwm, zjs_pwm_init)
#endif  // BUILD_MODULE_PWM
