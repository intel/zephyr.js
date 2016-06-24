// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>
#include <soletta.h>
#include <sol-gpio.h>
#include <sys_clock.h>
#include <string.h>

#include "jerry.h"
#include "jerry-api.h"
#include "util.h"
#include "script.h"                    // generated from blink.js

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#define PIN_COUNT 		30
static struct sol_gpio *gpio[PIN_COUNT];

// setInterval timer callback
static bool
timer_cb(void *data)
{
    bool is_ok;
    jerry_api_object_t *js_func = data;

    if (!jerry_api_is_function(js_func))
    {
        PRINT ("Error: callback is not a function!!!\n");
        return false;
    }

    jerry_api_value_t res;
    is_ok = jerry_api_call_function(js_func,
                                    NULL,
                                    &res,
                                    NULL,
                                    0);
    jerry_api_release_value(&res);

    return true;
}

// native setInterval handler
static bool
native_setInterval_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[0].type != JERRY_API_DATA_TYPE_OBJECT ||
        !jerry_api_value_is_function(&args_p[0]) ||
        args_p[1].type != JERRY_API_DATA_TYPE_FLOAT32)
    {
        PRINT ("native_setInterval_handler: invalid arguments\n");
        return false;
    }

    int interval = (int) args_p[1].u.v_float32;
    sol_timeout_add(interval, timer_cb, args_p[0].u.v_object);

    return true;
}

// native gpio_pin_configure handler
static bool
native_gpio_pin_configure_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32)
    {
        PRINT ("native_gpio_pin_configure_handler: invalid arguments\n");
        return false;
    }

    int pin = (int) args_p[0].u.v_float32;

    if (pin < 0 || pin >= PIN_COUNT) {
        PRINT ("native_gpio_pin_configure_handler: pin out of range\n");
        return true;
    }

    // configure GPIO output
    struct sol_gpio_config cfg = {
        SOL_SET_API_VERSION(.api_version = SOL_GPIO_CONFIG_API_VERSION, )
        .dir = SOL_GPIO_DIR_OUT,
    };

    gpio[pin] = sol_gpio_open(pin, &cfg);

    if (gpio[pin])
        PRINT("opening gpio pin=%d\n", pin);
    else
        PRINT("failed to open gpio pin=%d for writing.\n", pin);

    return true;
}


// native gpio_pin_write handler
static bool
native_gpio_pin_write_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32 ||
        args_p[1].type != JERRY_API_DATA_TYPE_FLOAT32)
    {
        PRINT ("native_gpio_pin_write_handler: invalid arguments\n");
        return false;
    }

    int pin = (int) args_p[0].u.v_float32;
    int value = (int) args_p[1].u.v_float32;

    if (pin < 0 || pin >= PIN_COUNT) {
        PRINT ("native_gpio_pin_write_handler: pin out of range\n");
        return true;
    }

    if (gpio[pin]) {
        //PRINT("writing to pin %d with value %d\n", pin, value);
        sol_gpio_write(gpio[pin], value);
        sol_gpio_close(gpio[pin]);
    }
    else
        PRINT("failed to open gpio pin=%d for writing.\n", pin);

    return true;
}

// native sleep handler
static bool
native_sleep_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[0].type != JERRY_API_DATA_TYPE_FLOAT32)
    {
        PRINT ("native_sleep_handler: invalid arguments\n");
        return false;
    }

    int num_of_ticks = ((int) args_p[0].u.v_float32) / 10;

    // put task to sleep 
    task_sleep(num_of_ticks);

    return true;
}

static void
startup(void)
{
    jerry_init(JERRY_FLAG_EMPTY);
    jerry_api_object_t *err_obj_p = NULL;
    size_t len = strlen((char *)script);

    PRINT ("App starting...\n");
    if (!jerry_parse((jerry_api_char_t *) script, len, &err_obj_p)) {
        PRINT ("jerry_parse(): failed to parse javascript\n");
        return;
    }

    jerry_api_object_t *global_obj = jerry_api_get_global();

    // create the C handler for setInterval JS call
    zjs_obj_add_function(global_obj, native_setInterval_handler, "setInterval");
    zjs_obj_add_function(global_obj, native_sleep_handler, "sleep");

    // create global GPIO object
    jerry_api_object_t *gpio_obj = jerry_api_create_object();
    zjs_obj_add_object(global_obj, gpio_obj, "GPIO");
    zjs_obj_add_function(gpio_obj, native_gpio_pin_configure_handler,
                         "pinConfigure");
    zjs_obj_add_function(gpio_obj, native_gpio_pin_write_handler,
                         "pinWrite");

    if (jerry_run(&err_obj_p) != JERRY_COMPLETION_CODE_OK) {
        PRINT ("jerry_run(): failed to run javascript\n");
        return;
    }
}

static void
shutdown(void)
{
    PRINT ("App exiting...\n");
}

SOL_MAIN_DEFAULT(startup, shutdown);
