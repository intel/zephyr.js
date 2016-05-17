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
#include "zjs_gpio.h"
#include "zjs_timers.h"
#include "zjs_util.h"

// local includes
#include "script.h"

void main(int argc, char *argv[])
{
    jerry_init(JERRY_FLAG_EMPTY);

    jerry_api_object_t *global_obj = jerry_api_get_global();
    zjs_gpio_init(global_obj);

    zjs_timers_init();

    jerry_api_object_t *err_obj_p = NULL;
    size_t len = strlen((char *)script);
    jerry_parse((jerry_api_char_t *)script, len, &err_obj_p);

    jerry_run(&err_obj_p);

    while (1) {
        if (zjs_timers_process_events() == false)
            break;
    }
}
