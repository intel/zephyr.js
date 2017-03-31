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
    struct requires_list *next;
} requires_list_t;

static requires_list_t *req_list = NULL;
static requires_list_t *loaded_list = NULL;

static bool is_loaded(const char *file_name)
{
    if(loaded_list) {
        requires_list_t *cur = loaded_list;
        while (cur) {
            if (strcmp(file_name, cur->file_name) != 0) {
                cur = cur->next;
            }
            else {
                return true;
            }
        }
    }
    return false;
}

static void push_to_list(requires_list_t **list, const char *item)
{
    requires_list_t *new = zjs_malloc(sizeof(requires_list_t));
    size_t len = strlen(item) + 1;
    new->file_name = (char*)zjs_malloc(len);
    strncpy(new->file_name, item, len);
    new->file_name[len] = '\0';
    new->next = *list;
    *list = new;
}

static void free_list(requires_list_t *list)
{
    if (list) {
        requires_list_t *cur = list;
        while (cur) {
            zjs_free(cur->file_name);
            list = list->next;
            zjs_free(cur);
            cur = list;
        }
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

static int read_file(const char *file_name, char **file_buf)
{
    fs_file_t *fp = fs_open_alloc(file_name, "r");

    if (fp == NULL)
        return 0;

    ssize_t size = fs_size(fp);
    if (size == 0) {
        comms_printf("[ERR] Empty file (%s)\n", file_name);
        fs_close_alloc(fp);
        return 0;
    }

    *file_buf = (char *)zjs_malloc(size);
    if (*file_buf == NULL) {
        comms_printf("[ERR] Not enough memory for (%s)\n", file_name);
        fs_close_alloc(fp);
        return 0;
    }

    ssize_t brw = fs_read(fp, *file_buf, size);

    if (brw != size) {
        comms_printf("[ERR] Failed loading code %s\n", file_name);
        fs_close_alloc(fp);
        return 0;
    }

    return size;
}

static void find_requires(char *filebuf)
{
    char *ptr1 = filebuf;
    char *ptr2 = NULL;
    char *filestr = NULL;
    char *ext_check = NULL;
    while(ptr1 != NULL){

        // Find next instance of "require"
        ptr1 = strstr(ptr1, "require");

        if (ptr1 != NULL) {
            // Find the text between the two " " after require
            ptr1 = strchr(ptr1, '"')+1;
            ptr2 = strchr(ptr1, '"');
            size_t len = ptr2-ptr1;

            // Allocate the memory for the string
            filestr = (char*)zjs_malloc(len+1);
            strncpy(filestr, ptr1, len);
            filestr[len] = '\0';

            // Check that this is a *.js require and not a zephyr require
            // Also check that the file hasn't already been loaded
            ext_check = &filestr[len-3];

            if (strcmp(ext_check, ".js") == 0 && !is_loaded(filestr))
            {
                push_to_list(&req_list, filestr);
            }
            zjs_free(filestr);
            filestr = NULL;
        }
    }
}

static void load_require_modules(char *file_buffer)
{
    // Check if the file we've been asked to run requires any JS modules
    find_requires(file_buffer);

    // If it does, check if they have requires as well, and then eval the code
    while (req_list) {
        char *req_file_buffer = NULL;
        uint file_size = read_file(req_list->file_name, &req_file_buffer);

        if (file_size > 0)
        {
            find_requires(req_file_buffer);
            javascript_eval_code(req_file_buffer, file_size);
            zjs_free(req_file_buffer);
            // Add file to the loaded list
            push_to_list(&loaded_list, req_list->file_name);
        }
        // Go to the next item in the require list
        requires_list_t *cur = req_list;
        zjs_free(cur->file_name);
        req_list = req_list->next;
        zjs_free(cur);
    }
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

void javascript_eval_code(const char *source_buffer, size_t size)
{
    jerry_port_default_set_log_level(JERRY_LOG_LEVEL_TRACE);
    ZVAL ret_val = jerry_eval((jerry_char_t *) source_buffer,
                              size,
                              false);

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
    zjs_timers_init();
#ifdef BUILD_MODULE_CONSOLE
    zjs_console_init();
#endif
#ifdef BUILD_MODULE_BUFFER
    zjs_buffer_init();
#endif
#ifdef BUILD_MODULE_SENSOR
    zjs_sensor_init();
#endif
    zjs_init_callbacks();
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
    zjs_ipm_free_callbacks();
    jerry_cleanup();

    restore_zjs_api();
}

int javascript_parse_code(const char *file_name, bool show_lines)
{
    int ret = -1;
    javascript_stop();
    jerry_port_default_set_log_level(JERRY_LOG_LEVEL_TRACE);
    char *buf = NULL;

    fs_file_t *fp = fs_open_alloc(file_name, "r");

    if (fp == NULL)
        return ret;

    ssize_t size = fs_size(fp);
    if (size == 0) {
        comms_printf("[ERR] Empty file (%s)\n", file_name);
        goto cleanup;
    }

    buf = (char *)zjs_malloc(size);
    if (buf == NULL) {
        comms_printf("[ERR] Not enough memory for (%s)\n", file_name);
        goto cleanup;
    }

    ssize_t brw = fs_read(fp, buf, size);

    if (brw != size) {
        comms_printf("[ERR] Failed loading code %s\n", file_name);
        goto cleanup;
    }

    if (show_lines) {
        comms_printf("[READ] %d\n", (int)brw);

        // Print buffer test
        int line = 0;
        comms_println("[START]");
        comms_printf("%5d  ", line++);
        for (int t = 0; t < brw; t++) {
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
    load_require_modules(buf);
    free_list(req_list);
    free_list(loaded_list);

    /* Setup Global scope code */
    parsed_code = jerry_parse((const jerry_char_t *) buf, size, false);
    if (jerry_value_has_error_flag(parsed_code)) {
        printf("[ERR] Could not parse JS\n");
        javascript_print_error(parsed_code);
        jerry_release_value(parsed_code);
    } else {
        ret = 0;
    }

cleanup:
    fs_close_alloc(fp);
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
