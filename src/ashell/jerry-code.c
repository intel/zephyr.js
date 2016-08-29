/*
 * Copyright 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief Simple interface to load and run javascript from our code memory stash
 *
 * Reads a program from the ROM/RAM
 */

/* Zephyr includes */
#include <zephyr.h>
#include <malloc.h>
#include <string.h>

/* JerryScript includes */
#include "jerry-api.h"

#include "acm-uart.h"
#include "file-wrapper.h"

static jerry_value_t parsed_code = 0;

#define MAX_BUFFER_SIZE 4096

void javascript_eval_code(const char *source_buffer)
{
    jerry_value_t ret_val;

    ret_val = jerry_eval((jerry_char_t *)source_buffer,
                         strnlen(source_buffer, MAX_BUFFER_SIZE),
                         false);

    if (jerry_value_has_error_flag(ret_val)) {
        printf("Failed to run JS\n");
    }

    jerry_release_value(ret_val);
}

void javascript_stop()
{
    if (parsed_code == 0)
        return;

    /* Parsed source code must be freed */
    jerry_release_value(parsed_code);
    parsed_code = 0;

    /* Cleanup engine */
    jerry_cleanup();

    /* Initialize engine */
    jerry_init(JERRY_INIT_EMPTY);
}

void javascript_run_code(const char *file_name)
{
    javascript_stop();

    ZFILE *fp = csopen(file_name, "r");
    if (fp == NULL)
        return;

    fs_seek(fp, 0, SEEK_END);
    off_t len = fs_tell(fp);
    if (len == 0) {
        printf("Empty file\n");
        return;
    }

    char *buf = (char *)malloc(len);

    fs_seek(fp, 0, SEEK_SET);
    ssize_t brw = fs_read(fp, buf, len);
    if (brw < 0) {
        fs_close(fp);
        printf(" Failed loading code from disk %s ", file_name);
        return;
    }

    /* Setup Global scope code */
    parsed_code = jerry_parse((const jerry_char_t *)buf, len, false);
    free(buf);

    if (!jerry_value_has_error_flag(parsed_code)) {
        /* Execute the parsed source code in the Global scope */
        jerry_value_t ret_value = jerry_run(parsed_code);

        /* Returned value must be freed */
        jerry_release_value(ret_value);
    } else {
        printf("JerryScript: could not parse javascript\n");
        return;
    }

}

void javascript_run_snapshot(const char *file_name)
{

}
