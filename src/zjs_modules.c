// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>
#include <stdlib.h>

// ZJS includes
#ifdef BUILD_MODULE_BUFFER
#include "zjs_buffer.h"
#endif
#ifdef BUILD_MODULE_CONSOLE
#include "zjs_console.h"
#endif
#include "zjs_dgram.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_performance.h"
#ifdef BUILD_MODULE_SENSOR
#include "zjs_sensor.h"
#endif
#include "zjs_timers.h"
#include "zjs_util.h"
#ifdef BUILD_MODULE_OCF
#include "zjs_ocf_common.h"
#endif
#ifdef BUILD_MODULE_TEST_PROMISE
#include "zjs_test_promise.h"
#endif

#ifndef ZJS_LINUX_BUILD
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_gpio.h"
#include "zjs_grove_lcd.h"
#include "zjs_i2c.h"
#include "zjs_pwm.h"
#include "zjs_uart.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_a101_pins.h"
#endif
#ifdef CONFIG_BOARD_FRDM_K64F
#include "zjs_k64f_pins.h"
#endif
#else
#include "zjs_script.h"
#endif // ZJS_LINUX_BUILD

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
    { "ble", zjs_ble_init, zjs_ble_cleanup },
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
#ifdef BUILD_MODULE_DGRAM
    { "dgram", zjs_dgram_init, zjs_dgram_cleanup },
#endif
#ifdef BUILD_MODULE_EVENTS
    { "events", zjs_event_init, zjs_event_cleanup },
#endif
#ifdef BUILD_MODULE_PERFORMANCE
    { "performance", zjs_performance_init },
#endif
#ifdef BUILD_MODULE_OCF
    { "ocf", zjs_ocf_init },
#endif
#ifdef BUILD_MODULE_TEST_PROMISE
    { "test_promise", zjs_test_promise_init }
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
    if (!jerry_value_is_string(argv[0])) {
        return TYPE_ERROR("native_require_handler: string expected");
    }

    const int MAX_MODULE_LEN = 32;
    jerry_size_t size = MAX_MODULE_LEN;
    char module[size];
    zjs_copy_jstring(argv[0], module, &size);
    if (!size) {
        return RANGE_ERROR("native_require_handler: argument too long");
    }

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
    DBG_PRINT("Native module not found, searching for JavaScript module %s\n",
              module);
#ifdef ZJS_LINUX_BUILD
    // Linux can pass in the script at runtime, so we have to read in/parse any
    // JS modules now rather than at compile time
    char full_path[size + 9];
    char* str;
    uint32_t len;
    sprintf(full_path, "modules/%s", module);
    full_path[size + 8] = '\0';

    if (zjs_read_script(full_path, &str, &len)) {
        ERR_PRINT("could not read module %s\n", full_path);
        return SYSTEM_ERROR("native_require_handler: could not read module script");
    }
    jerry_value_t code_eval = jerry_parse((jerry_char_t *)str, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        jerry_release_value(code_eval);
        return SYSTEM_ERROR("native_require_handler: could not parse javascript");
    }
    jerry_value_t result = jerry_run(code_eval);
    jerry_release_value(code_eval);
    if (jerry_value_has_error_flag(result)) {
        jerry_release_value(result);
        return SYSTEM_ERROR("native_require_handler: could not run javascript");
    }

    jerry_release_value(result);
    zjs_free_script(str);
#endif

    jerry_value_t global_obj = jerry_get_global_object();
    jerry_value_t modules_obj = zjs_get_property(global_obj, "module");
    jerry_release_value(global_obj);

    if (!jerry_value_is_object(modules_obj)) {
        jerry_release_value(modules_obj);
        return SYSTEM_ERROR("native_require_handler: modules object not found");
    }

    jerry_value_t exports_obj = zjs_get_property(modules_obj, "exports");
    jerry_release_value(modules_obj);

    if (!jerry_value_is_object(exports_obj)) {
        jerry_release_value(exports_obj);
        return SYSTEM_ERROR("native_require_handler: exports object not found");
    }

    for (int i = 0; i < 4; i++) {
        // Strip the ".js"
        module[size-i] = '\0';
    }

    jerry_value_t found_obj = zjs_get_property(exports_obj, module);
    jerry_release_value(exports_obj);

    if (!jerry_value_is_object(found_obj)) {
        jerry_release_value(found_obj);
        return NOTSUPPORTED_ERROR("native_require_handler: module not found");
    }

    DBG_PRINT("JavaScript module %s loaded\n", module);
    jerry_release_value(exports_obj);
    return found_obj;
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
    zjs_error_init();
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
    // stop timers first to prevent further calls
    zjs_timers_cleanup();

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
    zjs_error_cleanup();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_cleanup();
#endif
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

uint8_t zjs_service_routines(void)
{
    uint8_t serviced = 0;
    int i;
    for (i = 0; i < num_routines; ++i) {
        if (svc_routine_map[i].func(svc_routine_map[i].handle)) {
            serviced = 1;
        }
    }
    return serviced;
}
