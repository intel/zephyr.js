// Copyright (c) 2016, Intel Corporation.
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#include "zjs_zephyr_time.h"
#else
#include "zjs_linux_time.h"
#endif // ZJS_LINUX_BUILD
#include <string.h>
#include "zjs_script.h"

// JerryScript includes
#include "jerry-api.h"

// Platform agnostic modules/headers
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_modules.h"
#include "zjs_timers.h"
#include "zjs_util.h"

#ifndef ZJS_LINUX_BUILD
// ZJS includes
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_gpio.h"
#include "zjs_pwm.h"
#include "zjs_i2c.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_a101_pins.h"
#endif // ZJS_LINUX_BUILD
#ifdef CONFIG_BOARD_FRDM_K64F
#include "zjs_k64f_pins.h"
#endif
#endif

#ifndef ZJS_LINUX_BUILD
void main(void)
#else
void main(int argc, char *argv[])
#endif
{
    char* script = NULL;
    jerry_value_t code_eval;
    jerry_value_t result;
    uint32_t len;

    jerry_init(JERRY_INIT_EMPTY);

    zjs_timers_init();
#ifndef ZJS_LINUX_BUILD
    zjs_queue_init();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_init();
#endif
    zjs_init_callbacks();

    // initialize modules
    zjs_modules_init();

#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifndef CONFIG_BOARD_FRDM_K64F
#ifdef BUILD_MODULE_AIO
    zjs_modules_add("aio", zjs_aio_init);
#endif
#endif
#ifdef BUILD_MODULE_BLE
    zjs_modules_add("ble", zjs_ble_init);
#endif
#ifdef BUILD_MODULE_GPIO
    zjs_modules_add("gpio", zjs_gpio_init);
#endif
#ifdef BUILD_MODULE_PWM
    zjs_modules_add("pwm", zjs_pwm_init);
#endif
#ifdef BUILD_MODULE_I2C
    zjs_modules_add("i2c", zjs_i2c_init);
#endif
#ifdef CONFIG_BOARD_ARDUINO_101
#ifdef BUILD_MODULE_A101
    zjs_modules_add("arduino101_pins", zjs_a101_init);
#endif
#endif
#ifdef CONFIG_BOARD_FRDM_K64F
    zjs_modules_add("k64f_pins", zjs_k64f_init);
#endif
#endif // QEMU_BUILD
#endif // ZJS_LINUX_BUILD

#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        zjs_read_script(argv[1], &script, &len);
    } else {
        zjs_read_script(NULL, &script, &len);
    }
#else
    zjs_read_script(NULL, &script, &len);
#endif

    code_eval = jerry_parse((jerry_char_t *)script, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        PRINT("JerryScript: cannot parse javascript\n");
        return;
    }

    zjs_free_script(script);

    result = jerry_run(code_eval);
    if (jerry_value_has_error_flag(result)) {
        PRINT("JerryScript: cannot run javascript\n");
        return;
    }

    // Magic value set in JS to enable sleep during the loop
    // This is needed to make WebBluetooth demo work for now, but breaks all
    //   our other samples if we just do it all the time.
    jerry_value_t global_obj = jerry_get_global_object();
    jerry_release_value(global_obj);
    jerry_release_value(code_eval);
    jerry_release_value(result);

#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifdef BUILD_MODULE_BLE
    zjs_ble_enable();
#endif
#endif
#endif // ZJS_LINUX_BUILD

    while (1) {
        zjs_timers_process_events();
#ifndef ZJS_LINUX_BUILD
        zjs_run_pending_callbacks();
#endif
        zjs_service_callbacks();
        // not sure if this is okay, but it seems better to sleep than
        //   busy wait
        zjs_sleep(1);
    }
}
