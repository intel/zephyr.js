// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>
#include <stdlib.h>

// ZJS includes
#include "zjs_console.h"
#include "zjs_error.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_performance.h"
#include "zjs_timers.h"
#include "zjs_util.h"
#ifdef BUILD_MODULE_OCF
#include "zjs_ocf_common.h"
#endif
#ifndef ZJS_LINUX_BUILD
// ZJS includes
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_gpio.h"
#include "zjs_grove_lcd.h"
#include "zjs_i2c.h"
#include "zjs_pwm.h"
#include "zjs_uart.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_a101_pins.h"
#endif // ZJS_LINUX_BUILD
#ifdef CONFIG_BOARD_FRDM_K64F
#include "zjs_k64f_pins.h"
#endif
#endif

typedef jerry_value_t (*initcb_t)();
typedef void (*cleanupcb_t)();

typedef struct module {
    const char *name;
    initcb_t init;
    cleanupcb_t cleanup;
    jerry_value_t instance;
} module_t;

// init function is required, cleanup is optional in these entries
module_t zjs_modules_array[] = {
#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifndef CONFIG_BOARD_FRDM_K64F
#ifdef BUILD_MODULE_AIO
    { "aio", zjs_aio_init, zjs_aio_cleanup },
#endif
#endif
#ifdef BUILD_MODULE_BLE
    { "ble", zjs_ble_init },
#endif
#ifdef BUILD_MODULE_GPIO
    { "gpio", zjs_gpio_init, zjs_gpio_cleanup },
#endif
#ifdef BUILD_MODULE_GROVE_LCD
    { "grove_lcd", zjs_grove_lcd_init, zjs_grove_lcd_cleanup },
#endif
#ifdef BUILD_MODULE_PWM
    { "pwm", zjs_pwm_init },
#endif
#ifdef BUILD_MODULE_I2C
    { "i2c", zjs_i2c_init },
#endif
#ifdef CONFIG_BOARD_ARDUINO_101
#ifdef BUILD_MODULE_A101
    { "arduino101_pins", zjs_a101_init },
#endif
#endif
#ifdef CONFIG_BOARD_FRDM_K64F
    { "k64f_pins", zjs_k64f_init },
#endif
#endif // QEMU_BUILD
#ifdef BUILD_MODULE_UART
    { "uart", zjs_uart_init, zjs_uart_cleanup },
#endif
#endif // ZJS_LINUX_BUILD
#ifdef BUILD_MODULE_EVENTS
    { "events", zjs_event_init, zjs_event_cleanup },
#endif
#ifdef BUILD_MODULE_PERFORMANCE
    { "performance", zjs_performance_init },
#endif
#ifdef BUILD_MODULE_OCF
    { "ocf", zjs_ocf_init }
#endif
};

struct routine_map {
    zjs_service_routine func;
    void* handle;
};

static uint8_t num_routines = 0;
struct routine_map svc_routine_map[NUM_SERVICE_ROUTINES];

static jerry_value_t native_require_handler(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    jerry_value_t arg = argv[0];
    if (!jerry_value_is_string(arg)) {
        return zjs_error("native_require_handler: invalid argument");
    }

    const int maxlen = 32;
    char module[maxlen];
    jerry_size_t sz = jerry_get_string_size(arg);
    if (sz >= maxlen) {
        return zjs_error("native_require_handler: argument too long");
    }
    int len = jerry_string_to_char_buffer(arg, (jerry_char_t *)module, sz);
    module[len] = '\0';

    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];
        if (!strcmp(mod->name, module)) {
            // We only want one instance of each module at a time
            if (mod->instance == 0) {
                mod->instance = jerry_acquire_value(mod->init());
            }
            return mod->instance;
        }
    }
    ZJS_PRINT("Native module not found, searching for JavaScript module %s\n", module);
    jerry_value_t global_obj = jerry_get_global_object();
    jerry_value_t modules_obj = zjs_get_property(global_obj, "module");
    if (!jerry_value_is_object(modules_obj)) {
        jerry_release_value(global_obj);
        jerry_release_value(modules_obj);
        return zjs_error("native_require_handler: modules object not found");
    }
    jerry_value_t exports_obj = zjs_get_property(modules_obj, "exports");
    if (!jerry_value_is_object(exports_obj)) {
        jerry_release_value(global_obj);
        jerry_release_value(modules_obj);
        jerry_release_value(exports_obj);
        return zjs_error("native_require_handler: exports object not found");
    }

    for (int i = 0; i < 4; i++) {
        // Strip the ".js"
        module[len-i] = '\0';
    }

    jerry_value_t found_obj = zjs_get_property(exports_obj, module);

    if (jerry_value_is_object(found_obj)) {
        ZJS_PRINT("JavaScript module %s loaded\n", module);
        jerry_release_value(global_obj);
        jerry_release_value(modules_obj);
        jerry_release_value(exports_obj);
        return found_obj;
    }

    jerry_release_value(global_obj);
    jerry_release_value(modules_obj);
    jerry_release_value(exports_obj);
    return zjs_error("native_require_handler: module not found");
}

void zjs_modules_init()
{
    jerry_value_t global_obj = jerry_get_global_object();

    // create the C handler for require JS call
    zjs_obj_add_function(global_obj, native_require_handler, "require");
    jerry_release_value(global_obj);

    // auto-load the events module without waiting for require(); needed so its
    //   init function will run before it's used by UART, etc.
    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];

        // DEV: if you add another module name here, remove the break below
        if (!strcmp(mod->name, "events")) {
            mod->instance = jerry_acquire_value(mod->init());
            break;
        }
    }

    // initialize fixed modules
    zjs_timers_init();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_init();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_init();
#endif
#ifdef BUILD_MODULE_SENSOR
    zjs_sensor_init();
#endif
}

void zjs_modules_cleanup()
{
    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);
    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];
        if (mod->instance) {
            if (mod->cleanup) {
                mod->cleanup();
            }
            jerry_release_value(mod->instance);
            mod->instance = 0;
        }
    }

    // clean up fixed modules
    zjs_timers_cleanup();
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_cleanup();
#endif
#ifdef BUILD_MODULE_SENSOR
    zjs_sensor_cleanup();
#endif
}

void zjs_register_service_routine(void* handle, zjs_service_routine func)
{
    if (num_routines >= NUM_SERVICE_ROUTINES) {
        DBG_PRINT(("not enough space, increase NUM_SERVICE_ROUTINES\n"));
        return;
    }
    svc_routine_map[num_routines].handle = handle;
    svc_routine_map[num_routines].func = func;
    num_routines++;
    return;
}

void zjs_service_routines(void)
{
    int i;
    for (i = 0; i < num_routines; ++i) {
        svc_routine_map[i].func(svc_routine_map[i].handle);
    }
}
