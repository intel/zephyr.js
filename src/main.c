// Copyright (c) 2016-2017, Intel Corporation.

// C inclues
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include "zjs_zephyr_port.h"
#include <zephyr.h>
#else
#include "zjs_linux_port.h"
#endif  // ZJS_LINUX_BUILD
#include "zjs_script.h"
#include "zjs_util.h"
#ifdef ZJS_ASHELL
#include "ashell/comms-uart.h"
#endif

// JerryScript includes
#include "jerryscript.h"

// Platform agnostic modules/headers
#include "zjs_callbacks.h"
#include "zjs_error.h"
#include "zjs_modules.h"
#ifdef BUILD_MODULE_SENSOR
#include "zjs_sensor.h"
#endif
#include "zjs_timers.h"
#ifdef BUILD_MODULE_OCF
#include "zjs_ocf_common.h"
#endif
#ifdef BUILD_MODULE_BLE
#include "zjs_ble.h"
#endif
#ifdef ZJS_LINUX_BUILD
#include "zjs_unit_tests.h"
#endif
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_ipm.h"
#endif

#define ZJS_MAX_PRINT_SIZE 512

#ifdef ZJS_SNAPSHOT_BUILD
const uint32_t snapshot_bytecode[] = {
#include "zjs_snapshot_gen.h"
};
const size_t snapshot_len = sizeof(snapshot_bytecode);
#else
const char script_jscode[] = {
#include "zjs_script_gen.h"
};
#endif

#ifdef ZJS_LINUX_BUILD
// enabled if --noexit is passed to jslinux
static u8_t no_exit = 0;
// if > 0, jslinux will exit after this many milliseconds
static u32_t exit_after = 0;
static struct timespec exit_timer;

u8_t process_cmd_line(int argc, char *argv[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "--unittest", 10)) {
            // run unit tests
            zjs_run_unit_tests();
        } else if (!strncmp(argv[i], "--noexit", 8)) {
            no_exit = 1;
        } else if (!strncmp(argv[i], "-t", 2)) {
            if (i == argc - 1) {
                // no time argument, return error
                ERR_PRINT("no time argument given after '-t'\n");
                return 0;
            } else {
                char *str_time = argv[i + 1];
                exit_after = atoi(str_time);
                ZJS_PRINT("jslinux will terminate after %u milliseconds\n",
                          (unsigned int)exit_after);
                clock_gettime(CLOCK_MONOTONIC, &exit_timer);
            }
        }
    }
    return 1;
}
#endif

#ifndef ZJS_LINUX_BUILD
void main(void)
#else
int main(int argc, char *argv[])
#endif
{
#ifndef ZJS_SNAPSHOT_BUILD
#ifdef ZJS_LINUX_BUILD
    char *script = NULL;
#else
    const char *script = NULL;
#endif
    jerry_value_t code_eval;
    u32_t len;
#endif
#ifndef ZJS_LINUX_BUILD
    zjs_loop_init();
#endif
    jerry_value_t result;

    // print newline here to make it easier to find
    // the beginning of the program
    ZJS_PRINT("\n");

#ifdef ZJS_POOL_CONFIG
    zjs_init_mem_pools();
#ifdef DUMP_MEM_STATS
    zjs_print_pools();
#endif
#endif

    jerry_init(JERRY_INIT_EMPTY);

    // initialize modules
    zjs_modules_init();

#ifdef BUILD_MODULE_OCF
    zjs_register_service_routine(NULL, main_poll_routine);
#endif

#ifndef ZJS_SNAPSHOT_BUILD
#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        if (process_cmd_line(argc - 1, argv + 1) == 0) {
            ERR_PRINT("command line options error\n");
            goto error;
        }
        if (zjs_read_script(argv[1], &script, &len)) {
            ERR_PRINT("could not read script file %s\n", argv[1]);
            goto error;
        }
    } else
    // slightly tricky: reuse next section as else clause
#endif
    {
        len = strnlen(script_jscode, MAX_SCRIPT_SIZE);
#ifdef ZJS_LINUX_BUILD
        script = zjs_malloc(len + 1);
        memcpy(script, script_jscode, len);
        script[len] = '\0';
#else
        script = script_jscode;
#endif
        if (len == MAX_SCRIPT_SIZE) {
            ERR_PRINT("Script size too large! Increase MAX_SCRIPT_SIZE.\n");
            goto error;
        }
    }
#endif

#ifndef ZJS_SNAPSHOT_BUILD
    code_eval = jerry_parse((jerry_char_t *)script, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        DBG_PRINT("Error parsing JS\n");
        zjs_print_error_message(code_eval, ZJS_UNDEFINED);
        goto error;
    }
#endif

#ifdef ZJS_LINUX_BUILD
    zjs_free_script(script);
#endif

#ifdef ZJS_SNAPSHOT_BUILD
    result = jerry_exec_snapshot(snapshot_bytecode, snapshot_len, false);
#else
    result = jerry_run(code_eval);
#endif

    if (jerry_value_has_error_flag(result)) {
        DBG_PRINT("Error running JS\n");
        zjs_print_error_message(result, ZJS_UNDEFINED);
        goto error;
    }

    // NOTE: don't use ZVAL on these because we want to release them early, so
    //   they don't stick around for the lifetime of the app
#ifndef ZJS_SNAPSHOT_BUILD
    jerry_release_value(code_eval);
#endif

    jerry_release_value(result);

#ifdef ZJS_LINUX_BUILD
    u8_t last_serviced = 1;
#endif
#ifdef ZJS_ASHELL
    zjs_ashell_init();
#endif
    while (1) {
#ifdef ZJS_ASHELL
        zjs_ashell_process();
#endif
        s32_t wait_time = ZJS_TICKS_FOREVER;
        u8_t serviced = 0;

        // callback cannot return a wait time
        if (zjs_service_callbacks()) {
            // when this was only called at the end, if a callback created a
            //   timer, it would think there were no timers and block forever
            // FIXME: need to consider the chicken and egg problems here
            serviced = 1;
        }
        u64_t wait = zjs_timers_process_events();
        if (wait != ZJS_TICKS_FOREVER) {
            serviced = 1;
            wait_time = (wait < wait_time) ? wait : wait_time;
        }
        wait = zjs_service_routines();
        if (wait != ZJS_TICKS_FOREVER) {
            serviced = 1;
            wait_time = (wait < wait_time) ? wait : wait_time;
        }
        // callback cannot return a wait time
        if (zjs_service_callbacks()) {
            serviced = 1;
        }
#ifndef ZJS_LINUX_BUILD
        zjs_loop_block(wait_time);
#endif
#ifdef ZJS_LINUX_BUILD
        if (!no_exit) {
            // if the last and current loop had no pending "events" (timers or
            // callbacks) and --autoexit is enabled the program will terminate
            if (last_serviced == 0 && serviced == 0) {
                ZJS_PRINT("\njslinux: no more timers or callbacks found, exiting!\n");
                ZJS_PRINT("   * to run your script indefinitely, use --noexit\n");
                ZJS_PRINT("   * to run your script for a set timeout, use -t <ms>\n");
                return 0;
            }
        }
        if (exit_after != 0) {
            // an exit timeout was passed in
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            u32_t elapsed = (1000 * (now.tv_sec - exit_timer.tv_sec)) +
                ((now.tv_nsec / 1000000) - (exit_timer.tv_nsec / 1000000));
            if (elapsed >= exit_after) {
                ZJS_PRINT("%u milliseconds have passed, exiting!\n",
                          (unsigned int)elapsed);
                return 0;
            }
        }
        last_serviced = serviced;
#endif
    }
error:
#ifdef ZJS_LINUX_BUILD
    return 1;
#else
    return;
#endif
}
