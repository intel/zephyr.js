// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <string.h>

// JerryScript includes
#include "jerry.h"
#include "jerry-api.h"

// ZJS includes
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_buffer.h"
#include "zjs_common.h"
#include "zjs_gpio.h"
#include "zjs_modules.h"
#include "zjs_pwm.h"
#include "zjs_timers.h"
#include "zjs_util.h"
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_a101_pins.h"
#endif

extern const char script[];

void main(int argc, char *argv[])
{
    jerry_object_t *err_obj_p = NULL;
    jerry_value_t res;

    jerry_init(JERRY_FLAG_EMPTY);

    zjs_timers_init();
    zjs_queue_init();
    zjs_buffer_init();

    // initialize modules
    zjs_modules_init();
#ifndef QEMU_BUILD
    zjs_modules_add("aio", zjs_aio_init);
    zjs_modules_add("ble", zjs_ble_init);
    zjs_modules_add("gpio", zjs_gpio_init);
    zjs_modules_add("pwm", zjs_pwm_init);
    zjs_modules_add("arduino101_pins", zjs_a101_init);
#endif

    size_t len = strlen((char *) script);

    if (!jerry_parse((jerry_char_t *) script, len, &err_obj_p)) {
        PRINT("JerryScript: cannot parse javascript\n");
        return;
    }

    if (jerry_run(&res) != JERRY_COMPLETION_CODE_OK) {
        PRINT("JerryScript: cannot run javascript\n");
        return;
    }

    // Magic value set in JS to enable sleep during the loop
    // This is needed to make WebBluetooth demo work for now, but breaks all
    //   our other samples if we just do it all the time.
    jerry_object_t *global_obj = jerry_get_global();
    double dsleep;
    int32_t isleep = 0;
    if (zjs_obj_get_double(global_obj, "zjs_sleep", &dsleep)) {
        isleep = (int32_t)dsleep;
        if (isleep) {
            PRINT("Found magic sleep value: %ld!\n", isleep);
        }
    }
    jerry_release_object(global_obj);

#ifndef QEMU_BUILD
    zjs_ble_enable();
#endif
    while (1) {
        zjs_timers_process_events();
        if (isleep) {
            // sleep here temporary fixes the BLE bug
            task_sleep(isleep);
        }
        zjs_run_pending_callbacks();
        // not sure if this is okay, but it seems better to sleep than
        //   busy wait
        task_sleep(1);
    }
}
