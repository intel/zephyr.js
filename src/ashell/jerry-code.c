// Copyright (c) 2016-2017, Intel Corporation.

/**
 * @file
 * @brief Simple interface to load and run javascript from our code memory stash
 *
 * Reads a program from the ROM/RAM
 */

/* Zephyr includes */
#include <zephyr.h>
#include <string.h>
#include <ctype.h>

/* JerryScript includes */
#include "jerry-port.h"
#include "jerry-code.h"
#include "file-utils.h"

/* Zephyr.js init everything */
#include "../zjs_buffer.h"
#include "../zjs_callbacks.h"
#include "../zjs_ipm.h"
#include "../zjs_modules.h"
#include "../zjs_sensor.h"
#include "../zjs_timers.h"
#include "../zjs_util.h"

void jerry_port_default_set_log_level(jerry_log_level_t level); /** Inside jerry-port-default.h */

#include "comms-uart.h"

static jerry_value_t parsed_code = 0;

typedef struct requires_list {
    char *file_name;
    bool reqs_checked;
    struct requires_list *next;
    struct requires_list *prev;
} requires_list_t;

static bool list_contains(requires_list_t **list, const char *file_name)
{
    requires_list_t *cur = *list;
    while (cur) {
        if (strcmp(file_name, cur->file_name) != 0) {
            cur = cur->next;
        }
        else {
            return true;
        }
    }
    return false;
}

static void free_list(requires_list_t **list)
{
    requires_list_t *cur = *list;
    while (cur) {
        zjs_free(cur->file_name);
        *list = (*list)->next;
        zjs_free(cur);
        cur = *list;
    }
}

static requires_list_t *next_req_to_scan(requires_list_t **list)
{
    requires_list_t *cur = *list;
    while (cur) {
        if (!cur->reqs_checked) {
            return cur;
        }
        else {
            cur = cur->next;
        }
    }
    return NULL;
}

static void add_to_list(requires_list_t **list, const char *item)
{
    requires_list_t *new = zjs_malloc(sizeof(requires_list_t));
    size_t len = strlen(item) + 1;
    new->file_name = (char *)zjs_malloc(len);
    strncpy(new->file_name, item, len);
    new->file_name[len] = '\0';
    new->next = *list;
    new->reqs_checked = false;
    if (*list != NULL) {
        new->prev = (*list)->prev;
        if ((*list)->prev != NULL)
            (*list)->prev->next = new;
        else
            *list = new;
        (*list)->prev = new;
    }
    else {
        new->prev = NULL;
        *list = new;
    }
}

static void javascript_print_value(const jerry_value_t value)
{
    if (jerry_value_is_undefined(value)) {
        jerry_port_console("undefined");
    } else if (jerry_value_is_null(value)) {
        jerry_port_console("null");
    } else if (jerry_value_is_boolean(value)) {
        if (jerry_get_boolean_value(value)) {
            jerry_port_console("true");
        } else {
            jerry_port_console("false");
        }
    }
    /* Float value */
    else if (jerry_value_is_number(value)) {
        double val = jerry_get_number_value(value);
        // %lf prints an empty value :?
        jerry_port_console("Number [%d]\n", (int) val);
    }
    /* String value */
    else if (jerry_value_is_string(value)) {
        /* Determining required buffer size */
        jerry_size_t size = 0;
        char *str = zjs_alloc_from_jstring(value, &size);
        if (str) {
            jerry_port_console("%s", str);
            zjs_free(str);
        }
        else {
            jerry_port_console("[String too long]");
        }
    }
    /* Object reference */
    else if (jerry_value_is_object(value)) {
        jerry_port_console("[JS object]");
    }

    jerry_port_console("\n");
}

static char *read_file_alloc(const char *file_name, ssize_t *size)
{
    char *file_buf = NULL;
    fs_file_t *fp = fs_open_alloc(file_name, "r");

    if (fp == NULL)
        return 0;

    *size = fs_size(fp);
    if (*size == 0) {
        comms_printf("[ERR] Empty file (%s)\n", file_name);
        fs_close_alloc(fp);
        return NULL;
    }

    file_buf = (char *)zjs_malloc(*size);
    if (file_buf == NULL) {
        comms_printf("[ERR] Not enough memory for (%s)\n", file_name);
        fs_close_alloc(fp);
        return NULL;
    }

    ssize_t brw = fs_read(fp, file_buf, *size);

    if (brw != *size) {
        comms_printf("[ERR] Failed loading code %s\n", file_name);
        fs_close_alloc(fp);
        zjs_free(file_buf);
        return NULL;
    }

    fs_close_alloc(fp);
    return file_buf;
}

static void eval_list(requires_list_t **list)
{
    // This assumes the list is in the correct order for calling eval
    if (*list) {
        ssize_t file_size;
        char *req_file_buffer = NULL;
        requires_list_t *cur = *list;
        while (cur) {
            req_file_buffer = read_file_alloc(cur->file_name, &file_size);
            javascript_eval_code(req_file_buffer, file_size);
            zjs_free(req_file_buffer);
            zjs_free(cur->file_name);
            *list = (*list)->next;
            zjs_free(cur);
            cur = *list;
        }
    }
}

static bool add_requires(requires_list_t **list, char *filebuf)
{
    // Scan the file and if it contains requires, append them before *list
    char *ptr1 = filebuf;
    char *ptr2 = NULL;
    char *filestr = NULL;
    char *ext_check = NULL;

    while (ptr1 != NULL) {
        // Find next instance of "require"
        ptr1 = strstr(ptr1, "require");

        if (ptr1 != NULL) {
            // Find the text between the two " " after require
            ptr1 = strchr(ptr1, '"') + 1;
            ptr2 = strchr(ptr1, '"');
            size_t len = ptr2 - ptr1;
            if (len < (ssize_t)MAX_ASHELL_JS_MODULE_LEN) {
                // Allocate the memory for the string
                filestr = (char *)zjs_malloc(len + 1);
                strncpy(filestr, ptr1, len);
                filestr[len] = '\0';

                // Check that this is a *.js require and not a zephyr require
                // Also check that the file hasn't already been loaded
                ext_check = &filestr[len - 3];

                if (strcmp(ext_check, ".js") == 0 && !list_contains(list, filestr)) {
                    add_to_list(list, filestr);
                }
                zjs_free(filestr);
                filestr = NULL;
            }
            else {
                filestr = (char *)zjs_malloc(10);
                strncpy(filestr, ptr1, 10);
                comms_printf("[ERR] requires(\"%s...\") string is too long\n", filestr);
                zjs_free(filestr);
                filestr = NULL;
                return false;
            }
        }
    }
    return true;
}

static bool load_require_modules(char *file_buffer)
{
    ssize_t file_size;
    char *req_file_buffer = NULL;
    requires_list_t *req_list = NULL;
    // Check if the file we've been asked to run requires any JS modules
    bool add_success = add_requires(&req_list, file_buffer);
    if (add_success) {
        // If it has any requires, check if those files have requires as well
        requires_list_t *cur = next_req_to_scan(&req_list);
        while (cur && add_success) {
            // Read the file into a buffer so we can scan it
            req_file_buffer = read_file_alloc(cur->file_name, &file_size);

            if (req_file_buffer && file_size > 0) {
                // If adding a requires fails, bail out of the while loop
                add_success = add_requires(&cur, req_file_buffer);
                cur->reqs_checked = true;
                zjs_free(req_file_buffer);
                if (!add_success) {
                    free_list(&req_list);
                    return false;
                }
            }
            // Get the next file to scan
            cur = next_req_to_scan(&req_list);
        }
        // The eval file list is complete, now jerry_eval all the files
        eval_list(&req_list);
        return true;
    }
    return false;
}

static void javascript_print_error(jerry_value_t error_value)
{
    if (!jerry_value_has_error_flag(error_value))
        return;

    jerry_value_clear_error_flag(&error_value);
    ZVAL err_str_val = jerry_value_to_string(error_value);

    jerry_size_t size = 0;
    char *msg = zjs_alloc_from_jstring(err_str_val, &size);
    const char *err_str = msg;
    if (!msg) {
        err_str = "[Error message too long]";
    }

    jerry_port_log(JERRY_LOG_LEVEL_ERROR, "%s\n", err_str);
    zjs_free(msg);
}

void javascript_eval_code(const char *source_buffer, ssize_t size)
{
    jerry_port_default_set_log_level(JERRY_LOG_LEVEL_TRACE);
    ZVAL ret_val = jerry_eval((jerry_char_t *) source_buffer, size, false);

    if (jerry_value_has_error_flag(ret_val)) {
        printf("[ERR] failed to evaluate JS\n");
        javascript_print_error(ret_val);
    } else {
        if (!jerry_value_is_undefined(ret_val))
            javascript_print_value(ret_val);
    }
}

void restore_zjs_api() {
#ifdef ZJS_POOL_CONFIG
    zjs_init_mem_pools();
#ifdef DUMP_MEM_STATS
    zjs_print_pools();
#endif
#endif
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

int javascript_parse_code(const char *file_name, bool show_lines)
{
    int ret = -1;
    javascript_stop();
    jerry_port_default_set_log_level(JERRY_LOG_LEVEL_TRACE);
    char *buf = NULL;
    ssize_t size;

    buf = read_file_alloc(file_name, &size);

    if (buf && size > 0) {
        if (show_lines) {
            comms_printf("[READ] %d\n", (int)size);

            // Print buffer test
            int line = 0;
            comms_println("[START]");
            comms_printf("%5d  ", line++);
            for (int t = 0; t < size; t++) {
                uint8_t byte = buf[t];
                if (byte == '\n' || byte == '\r') {
                    comms_write_buf("\r\n", 2);
                    comms_printf("%5d  ", line++);
                } else {
                    if (!isprint(byte)) {
                        comms_printf("(%x)", byte);
                    } else
                        comms_writec(byte);
                }
            }
            comms_println("[END]");
        }

        // Find and load all required js modules
        if (load_require_modules(buf)) {
            /* Setup Global scope code */
            parsed_code = jerry_parse((const jerry_char_t *) buf, size, false);
            if (jerry_value_has_error_flag(parsed_code)) {
                printf("[ERR] Could not parse JS\n");
                javascript_print_error(parsed_code);
                jerry_release_value(parsed_code);
            } else {
                ret = 0;
            }
        }
    }

    zjs_free(buf);
    return ret;
}

void javascript_run_code(const char *file_name)
{
    if (javascript_parse_code(file_name, false) != 0)
        return;

    /* Execute the parsed source code in the Global scope */
    ZVAL ret_value = jerry_run(parsed_code);

    if (jerry_value_has_error_flag(ret_value)) {
        javascript_print_error(ret_value);
    }
}
