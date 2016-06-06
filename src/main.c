// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#include <string.h>

// JerryScript includes
#include "jerry.h"
#include "jerry-api.h"

// ZJS includes
#include "zjs_aio.h"
#include "zjs_ble.h"
#include "zjs_gpio.h"
#include "zjs_modules.h"
#include "zjs_pwm.h"
#include "zjs_timers.h"
#include "zjs_util.h"

// local includes
#include "script.h"

void main(int argc, char *argv[])
{
    jerry_object_t *err_obj_p = NULL;
    jerry_value_t res;

    jerry_init(JERRY_FLAG_EMPTY);

    zjs_timers_init();
    zjs_queue_init();

    // Initializes modules stuffs...
    zjs_modules_init();
    zjs_modules_add("aio", zjs_aio_init);
    zjs_modules_add("ble", zjs_ble_init);
    zjs_modules_add("gpio", zjs_gpio_init);
    zjs_modules_add("pwm", zjs_pwm_init);

    size_t len = strlen((char *) script);

    if (!jerry_parse((jerry_char_t *) script, len, &err_obj_p)) {
        PRINT("JerryScript: cannot parse javascript\n");
        return;
    }

    if (jerry_run(&res) != JERRY_COMPLETION_CODE_OK) {
        PRINT("JerryScript: cannot run javascript\n");
        return;
    }

    while (1) {
        zjs_timers_process_events();
        zjs_run_pending_callbacks();
        // not sure if this is okay, but it seems better to sleep than
        //   busy wait
        task_sleep(1);
    }
}
