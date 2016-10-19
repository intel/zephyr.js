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

#ifdef ZJS_POOL_CONFIG
#include "zjs_pool.h"
#endif

// Platform agnostic modules/headers
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_console.h"
#include "zjs_event.h"
#include "zjs_modules.h"
#include "zjs_timers.h"
#include "zjs_util.h"

#include "zjs_ble.h"

extern const char *script_gen;

// native eval handler
static jerry_value_t native_eval_handler(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    return zjs_error("native_eval_handler: eval not supported");
}

#ifndef ZJS_LINUX_BUILD
void main(void)
#else
int main(int argc, char *argv[])
#endif
{
    const char *script = NULL;
    jerry_value_t code_eval;
    jerry_value_t result;
    uint32_t len;

    // print newline here to make it easier to find
    // the beginning of the program
    PRINT("\n");

#ifdef ZJS_POOL_CONFIG
    zjs_init_mem_pools();
#ifdef DUMP_MEM_STATS
    zjs_print_pools();
#endif
#endif

    jerry_init(JERRY_INIT_EMPTY);

    zjs_timers_init();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_init();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_init();
#endif
    zjs_init_callbacks();

    // initialize modules
    zjs_modules_init();

#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        zjs_read_script(argv[1], &script, &len);
    } else
    // slightly tricky: reuse next section as else clause
#endif
    {
        script = script_gen;
        len = strnlen(script_gen, MAX_SCRIPT_SIZE);
        if (len == MAX_SCRIPT_SIZE) {
            PRINT("Error: Script size too large! Increase MAX_SCRIPT_SIZE.\n");
            goto error;
        }
    }

    jerry_value_t global_obj = jerry_get_global_object();

    // Todo: find a better solution to disable eval() in JerryScript.
    // For now, just inject our eval() function in the global space
    zjs_obj_add_function(global_obj, native_eval_handler, "eval");

    code_eval = jerry_parse((jerry_char_t *)script, len, false);
    if (jerry_value_has_error_flag(code_eval)) {
        PRINT("JerryScript: cannot parse javascript\n");
        goto error;
    }

#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        zjs_free_script(script);
    }
#endif

    result = jerry_run(code_eval);
    if (jerry_value_has_error_flag(result)) {
        PRINT("JerryScript: cannot run javascript\n");
        goto error;
    }

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
        zjs_service_callbacks();
        // not sure if this is okay, but it seems better to sleep than
        //   busy wait
        zjs_sleep(1);
    }

error:
#ifdef ZJS_LINUX_BUILD
    return 1;
#else
    return;
#endif
}
