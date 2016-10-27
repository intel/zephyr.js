// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif
#include <string.h>
#include <stdlib.h>

// ZJS includes
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_util.h"

#ifndef ZJS_LINUX_BUILD
// ZJS includes
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_gpio.h"
#include "zjs_grove_lcd.h"
#include "zjs_pwm.h"
#include "zjs_i2c.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_a101_pins.h"
#endif // ZJS_LINUX_BUILD
#ifdef CONFIG_BOARD_FRDM_K64F
#include "zjs_k64f_pins.h"
#endif
#endif

typedef struct module {
    const char *name;
    initcb_t init;
    jerry_value_t instance;
} module_t;

module_t zjs_modules_array[] = {
#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifndef CONFIG_BOARD_FRDM_K64F
#ifdef BUILD_MODULE_AIO
    { "aio", zjs_aio_init },
#endif
#endif
#ifdef BUILD_MODULE_BLE
    { "ble", zjs_ble_init },
#endif
#ifdef BUILD_MODULE_GPIO
    { "gpio", zjs_gpio_init },
#endif
#ifdef BUILD_MODULE_GROVE_LCD
    { "grove_lcd", zjs_grove_lcd_init },
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
#endif // ZJS_LINUX_BUILD

#ifdef BUILD_MODULE_EVENTS
    { "events", zjs_event_init },
#endif
};

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
            // We only want one intance of each module at a time
            if (mod->instance == 0) {
                mod->instance = jerry_acquire_value(mod->init());
            }
            return mod->instance;
        }
    }

    PRINT("MODULE: `%s'\n", module);
    return zjs_error("native_require_handler: module not found");
}

void zjs_modules_init()
{
    int modcount = sizeof(zjs_modules_array) / sizeof(module_t);

    for (int i = 0; i < modcount; i++) {
        module_t *mod = &zjs_modules_array[i];
        mod->instance = 0;
    }

    jerry_value_t global_obj = jerry_get_global_object();

    // create the C handler for require JS call
    zjs_obj_add_function(global_obj, native_require_handler, "require");
}
