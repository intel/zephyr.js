// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif // ZJS_LINUX_BUILD
#include <string.h>
#include "zjs_script.h"

// JerryScript includes
#include "jerry-api.h"

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
#if defined(CONFIG_NET_L2_BLUETOOTH)
#include "zjs_ocf_ble.h"
#endif
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

#define ZJS_MAX_PRINT_SIZE      512

#ifdef ZJS_SNAPSHOT_BUILD
extern const uint8_t snapshot_bytecode[];
extern const int snapshot_len;
#else
extern const char *script_gen;
#endif

// native eval handler
static jerry_value_t native_eval_handler(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    return zjs_error("eval not supported");
}

// native print handler
static jerry_value_t native_print_handler(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(argv[0]))
        return zjs_error("print: missing string argument");

    jerry_size_t size = 0;
    char *str = zjs_alloc_from_jstring(argv[0], &size);
    if (!str)
        return zjs_error("print: out of memory");

    ZJS_PRINT("%s\n", str);
    zjs_free(str);
    return ZJS_UNDEFINED;
}

static jerry_value_t stop_js_handler(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    #ifdef CONFIG_BOARD_ARDUINO_101
    zjs_ipm_free_callbacks();
    #endif
    zjs_modules_cleanup();
    jerry_cleanup();
    return ZJS_UNDEFINED;
}

#ifdef ZJS_LINUX_BUILD
// enabled if --autoexit is passed to jslinux
static uint8_t auto_exit = 0;
// if > 0, jslinux will exit after this many milliseconds
static uint32_t exit_after = 0;
static struct timespec exit_timer;

uint8_t process_cmd_line(int argc, char *argv[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "--unittest", 10)) {
            // run unit tests
            zjs_run_unit_tests();
        }
        else if (!strncmp(argv[i], "--autoexit", 10)) {
            auto_exit = 1;
        }
        else if (!strncmp(argv[i], "-t", 2)) {
            if (i == argc - 1) {
                // no time argument, return error
                ERR_PRINT("no time argument given after '-t'\n");
                return 0;
            } else {
                char* str_time = argv[i + 1];
                exit_after = atoi(str_time);
                ZJS_PRINT("jslinux will terminate after %lu milliseconds\n",
                          exit_after);
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
    const char *script = NULL;
    jerry_value_t code_eval;
    uint32_t len;
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

    zjs_init_callbacks();

    // Add module.exports to global namespace
    jerry_value_t global_obj = jerry_get_global_object();
    jerry_value_t modules_obj = jerry_create_object();
    jerry_value_t exports_obj = jerry_create_object();

    zjs_set_property(modules_obj, "exports", exports_obj);
    zjs_set_property(global_obj, "module", modules_obj);

    // initialize modules
    zjs_modules_init();

    zjs_error_init();

#ifdef BUILD_MODULE_OCF
    zjs_register_service_routine(NULL, main_poll_routine);
#if defined(CONFIG_NET_L2_BLUETOOTH)
    zjs_init_ocf_ble();
#endif
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
            return -1;
        }
    } else
    // slightly tricky: reuse next section as else clause
#endif
    {
        script = script_gen;
        len = strnlen(script_gen, MAX_SCRIPT_SIZE);
        if (len == MAX_SCRIPT_SIZE) {
            ERR_PRINT("Script size too large! Increase MAX_SCRIPT_SIZE.\n");
            goto error;
        }
    }
#endif

    // Todo: find a better solution to disable eval() in JerryScript.
    // For now, just inject our eval() function in the global space
    zjs_obj_add_function(global_obj, native_eval_handler, "eval");
    zjs_obj_add_function(global_obj, native_print_handler, "print");
    zjs_obj_add_function(global_obj, stop_js_handler, "stopJS");
#ifndef ZJS_SNAPSHOT_BUILD
    code_eval = jerry_parse((jerry_char_t *)script, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        ERR_PRINT("JerryScript: cannot parse javascript\n");
        goto error;
    }
#endif

#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        zjs_free_script(script);
    }
#endif

#ifdef ZJS_SNAPSHOT_BUILD
    result = jerry_exec_snapshot(snapshot_bytecode,
                                 snapshot_len,
                                 false);
#else
    result = jerry_run(code_eval);
#endif

    if (jerry_value_has_error_flag(result)) {
        ERR_PRINT("JerryScript: cannot run javascript\n");
        goto error;
    }

#ifndef ZJS_SNAPSHOT_BUILD
    jerry_release_value(code_eval);
#endif
    jerry_release_value(global_obj);
    jerry_release_value(modules_obj);
    jerry_release_value(exports_obj);
    jerry_release_value(result);

#ifndef ZJS_LINUX_BUILD
#ifndef QEMU_BUILD
#ifdef BUILD_MODULE_BLE
    zjs_ble_enable();
#endif
#endif
#endif // ZJS_LINUX_BUILD

#ifdef ZJS_LINUX_BUILD
    uint8_t last_serviced = 1;
#endif

    while (1) {
        uint8_t serviced = 0;

        if (zjs_timers_process_events()) {
            serviced = 1;
        }
        if (zjs_service_callbacks()) {
            serviced = 1;
        }
        if (zjs_service_routines()) {
            serviced = 1;
        }
        // not sure if this is okay, but it seems better to sleep than
        //   busy wait
        zjs_sleep(1);
#ifdef ZJS_LINUX_BUILD
        if (auto_exit) {
            // if the last and current loop had no pending "events" (timers or
            // callbacks) and --autoexit is enabled the program will terminate
            if (last_serviced == 0 && serviced == 0) {
                ZJS_PRINT("no timers or callbacks were processed, exiting!\n");
                goto error;
            }
        }
        if (exit_after != 0) {
            // an exit timeout was passed in
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            uint32_t elapsed = (1000 * (now.tv_sec - exit_timer.tv_sec)) +
                    ((now.tv_nsec / 1000000) - (exit_timer.tv_nsec / 1000000));
            if (elapsed >= exit_after) {
                ZJS_PRINT("%lu milliseconds have passed, exiting!\n", elapsed);
                goto error;
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
