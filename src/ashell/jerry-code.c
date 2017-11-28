// Copyright (c) 2016-2017, Intel Corporation.

/**
 * @file
 * @brief Simple interface to load and run javascript from our code memory stash
 *
 * Reads a program from the ROM/RAM
 */

// C includes
#include <ctype.h>
#include <string.h>

// Zephyr includes
#include <zephyr.h>

// JerryScript includes
#include "file-utils.h"
#include "jerry-code.h"
#include "jerryscript-port.h"

// ZJS includes
#include "../zjs_buffer.h"
#include "../zjs_callbacks.h"
#include "../zjs_ipm.h"
#include "../zjs_modules.h"
#include "../zjs_sensor.h"
#include "../zjs_timers.h"
#include "../zjs_util.h"

#include "term-uart.h"

static jerry_value_t parsed_code = 0;

void javascript_eval_code(const char *source_buffer, ssize_t size)
{
    ZVAL ret_val = jerry_eval((jerry_char_t *)source_buffer, size, false);
    if (jerry_value_has_error_flag(ret_val)) {
        printf("[ERR] failed to evaluate JS\n");
        zjs_print_error_message(ret_val, ZJS_UNDEFINED);
    }
}

void restore_zjs_api()
{
    jerry_init(JERRY_INIT_EMPTY);
    zjs_modules_init();
}

void javascript_stop()
{
    if (parsed_code == 0)
        return;

    /* Parsed source code must be freed */
    jerry_release_value(parsed_code);
    parsed_code = 0;

    /* Cleanup engine */
    zjs_modules_cleanup();
    zjs_remove_all_callbacks();
#ifdef CONFIG_BOARD_ARDUINO_101
    zjs_ipm_free_callbacks();
#endif
    jerry_cleanup();

    restore_zjs_api();
}

int javascript_parse_code(const char *file_name)
{
    int ret = -1;
    javascript_stop();
    char *buf = NULL;
    ssize_t size;

    buf = read_file_alloc(file_name, &size);

    if (buf && size > 0) {
        /* Setup Global scope code */
        parsed_code = jerry_parse((const jerry_char_t *)buf, size, false);
        if (jerry_value_has_error_flag(parsed_code)) {
            DBG_PRINT("Error parsing JS\n");
            zjs_print_error_message(parsed_code, ZJS_UNDEFINED);
            jerry_release_value(parsed_code);
        } else {
            ret = 0;
        }
    }

    zjs_free(buf);
    return ret;
}

void javascript_run_code(const char *file_name)
{
    if (javascript_parse_code(file_name) != 0)
        return;

    /* Execute the parsed source code in the Global scope */
    ZVAL ret_value = jerry_run(parsed_code);

    if (jerry_value_has_error_flag(ret_value)) {
        DBG_PRINT("Error running JS\n");
        zjs_print_error_message(ret_value, ZJS_UNDEFINED);
    }
}
