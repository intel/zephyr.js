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
#include "jerryscript-port.h"
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
        jerry_port_log(JERRY_LOG_LEVEL_TRACE, "undefined");
    } else if (jerry_value_is_null(value)) {
        jerry_port_log(JERRY_LOG_LEVEL_TRACE, "null");
    } else if (jerry_value_is_boolean(value)) {
        if (jerry_get_boolean_value(value)) {
            jerry_port_log(JERRY_LOG_LEVEL_TRACE, "true");
        } else {
            jerry_port_log(JERRY_LOG_LEVEL_TRACE, "false");
        }
    }
    /* Float value */
    else if (jerry_value_is_number(value)) {
        double val = jerry_get_number_value(value);
        // %lf prints an empty value :?
        jerry_port_log(JERRY_LOG_LEVEL_TRACE, "Number [%d]\n", (int) val);
    }
    /* String value */
    else if (jerry_value_is_string(value)) {
        /* Determining required buffer size */
        jerry_size_t size = 0;
        char *str = zjs_alloc_from_jstring(value, &size);
        if (str) {
            jerry_port_log(JERRY_LOG_LEVEL_TRACE, "%s", str);
            zjs_free(str);
        }
        else {
            jerry_port_log(JERRY_LOG_LEVEL_TRACE, "[String too long]");
        }
    }
    /* Object reference */
    else if (jerry_value_is_object(value)) {
        jerry_port_log(JERRY_LOG_LEVEL_TRACE, "[JS object]");
    }

    jerry_port_log(JERRY_LOG_LEVEL_TRACE, "\n");
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

static void skip_whitespace(char **ptr)
{
    // modifies: *ptr
    //  effects: increments *ptr until it's past any whitespace (space, tab,
    //             newline) or gets to null terminator
    while (**ptr != '\0') {
        if (**ptr == ' ' || **ptr == '\t' || **ptr == '\r' || **ptr == '\n') {
            ++(*ptr);
        }
        else {
            return;
        }
    }
}

static bool skip_char_with_whitespace(char **ptr, char match)
{
    // modifies: *ptr
    //  effects: skips any whitespace in *ptr before and after a match char or
    //             until null terminator is reached
    //           if first non-whitespace char found is match, returns true;
    //             otherwise, false
    skip_whitespace(ptr);
    if (**ptr != match) {
        return false;
    }
    ++(*ptr);
    skip_whitespace(ptr);
    return true;
}

static char *find_next_require(char *source, char *module, int *modlen)
{
    // requires: source is a pointer into a null-terminated source buffer;
    //             module is a buffer with at least MAX_MODULE_STR_LEN bytes
    // modifies: module, if one is found, *modlen if not NULL
    //  effects: finds the next require('modname') in the source, writes the
    //             modname to module and returns pointer into source beyond the
    //             require, or NULL if not found; *modlen will be length of the
    //             module string
    char *ptr = source;
    while (1) {
        // find the word require
        // FIXME: buggy, doesn't check for comments, etc.
        char *require = strstr(ptr, "require");
        if (!require) {
            return NULL;
        }

        ptr = require + 7;
        if (!skip_char_with_whitespace(&ptr, '(')) {
            // no open paren found, not a valid require() call
            continue;
        }

        char quotechar;
        if (*ptr != '\'' && *ptr != '"') {
            // not a literal string we can handle
            continue;
        }
        quotechar = *ptr;

        char *modname = ++ptr;
        char *closechar = strchr(ptr, quotechar);
        if (!closechar) {
            // no matching quote found, shouldn't happen, try again
            continue;
        }

        ptr = closechar + 1;
        int len = closechar - modname;
        if (len <= 0 || len > MAX_MODULE_STR_LEN - 1) {
            // bogus module name, try again
            continue;
        }

        if (!skip_char_with_whitespace(&ptr, ')')) {
            // no close paren found, not a valid require() call
            continue;
        }

        strncpy(module, modname, len);
        module[len] = '\0';
        if (modlen) {
            *modlen = len;
        }
        return ptr;
    }
}

static bool add_requires(requires_list_t **list, char *filebuf)
{
    // Scan the file and if it contains requires, append them before *list
    char *ptr = filebuf;
    char filestr[MAX_MODULE_STR_LEN];
    int filelen = 0;

    while (1) {
        ptr = find_next_require(ptr, filestr, &filelen);
        if (!ptr) {
            return true;
        }

        // Check that this is a *.js require and not a zephyr require
        // Also check that the file hasn't already been loaded

        // FIXME: this always expects JS file to have .js lowercase extension
        if (filelen >= 3 && !strcmp(&filestr[filelen - 3], ".js") &&
            !list_contains(list, filestr)) {
            if (fs_valid_filename(filestr)) {
                add_to_list(list, filestr);
            }
            else {
                return false;
            }
        }
    }
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
            else {
                free_list(&req_list);
                return false;
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

int javascript_parse_code(const char *file_name)
{
    int ret = -1;
    javascript_stop();
    char *buf = NULL;
    ssize_t size;

    buf = read_file_alloc(file_name, &size);

    if (buf && size > 0) {
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
    if (javascript_parse_code(file_name) != 0)
        return;

    /* Execute the parsed source code in the Global scope */
    ZVAL ret_value = jerry_run(parsed_code);

    if (jerry_value_has_error_flag(ret_value)) {
        javascript_print_error(ret_value);
    }
}
