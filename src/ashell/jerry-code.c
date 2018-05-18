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
#include "jerryscript-port.h"

// ZJS includes
#include "zjs_file_utils.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_ipm.h"
#include "zjs_modules.h"
#include "zjs_sensor.h"
#include "zjs_timers.h"
#include "zjs_util.h"
#include "jerry-code.h"

#include "term-uart.h"

static jerry_value_t parsed_code = 0;

void javascript_eval_code(const char *source_buffer, ssize_t size)
{
    ZVAL ret_val = jerry_eval((jerry_char_t *)source_buffer, size, false);
    if (jerry_value_is_error(ret_val)) {
        printf("[ERR] failed to evaluate JS\n");
        zjs_print_error_message(ret_val, ZJS_UNDEFINED);
    }
}

void javascript_stop()
{
    if (parsed_code == 0)
        return;

    /* Parsed source code must be freed */
    jerry_release_value(parsed_code);
    parsed_code = 0;
    zjs_stop_js();
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
        parsed_code = jerry_parse(NULL, 0, (const jerry_char_t *)buf, size,
                                  JERRY_PARSE_NO_OPTS);
        if (jerry_value_is_error(parsed_code)) {
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

    if (jerry_value_is_error(ret_value)) {
        DBG_PRINT("Error running JS\n");
        zjs_print_error_message(ret_value, ZJS_UNDEFINED);
    }
}
