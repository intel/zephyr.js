// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_CONSOLE

// ZJS includes
#include "zjs_common.h"
#include "zjs_error.h"
#include "zjs_util.h"
#ifdef ZJS_LINUX_BUILD
#include "zjs_linux_port.h"
#else
#include "zjs_zephyr_port.h"
#endif

#ifdef JERRY_DEBUGGER
#include "debugger.h"
#endif

#define MAX_STR_LENGTH 256

#ifdef ZJS_LINUX_BUILD
#define STDERR_PRINT(s, ...) fprintf(stderr, s, __VA_ARGS__)
#define STDOUT_PRINT(s, ...) fprintf(stdout, s, __VA_ARGS__)
#else
#define STDERR_PRINT(s, ...) ZJS_PRINT(s, __VA_ARGS__)
#define STDOUT_PRINT(s, ...) ZJS_PRINT(s, __VA_ARGS__)
#endif

#define IS_NUMBER 0
#define IS_INT    1
#define IS_UINT   2

static jerry_value_t gbl_time_obj;

static int is_int(jerry_value_t val)
{
    int ret = 0;
    double n = jerry_get_number_value(val);
    ret = (n - (int)n == 0);
    if (ret) {
        // Integer type
        if (n < 0) {
            return IS_INT;
        } else {
            return IS_UINT;
        }
    } else {
        return IS_NUMBER;
    }
}

static bool value2str(const jerry_value_t value, char *buf, int maxlen,
                      bool quotes)
{
    // requires: buf has at least maxlen characters
    //  effects: writes a string representation of the value to buf; when
    //             processing a string value, puts quotes around it if quotes
    //             is true
    //  returns: true if the representation was complete or false if it
    //             was abbreviated
    ZVAL_MUTABLE str_val = ZJS_UNDEFINED;
    bool is_string = false;

    if (jerry_value_is_array(value)) {
        u32_t len = jerry_get_array_length(value);
        sprintf(buf, "[Array - length %u]", len);
        return false;
    } else if (jerry_value_is_boolean(value)) {
        u8_t val = jerry_get_boolean_value(value);
        sprintf(buf, (val) ? "true" : "false");
    } else if (jerry_value_is_function(value)) {
        sprintf(buf, "[Function]");
    } else if (jerry_value_is_number(value)) {
        int type = is_int(value);
        if (type == IS_NUMBER) {
            str_val = jerry_value_to_string(value);
            is_string = true;
        } else if (type == IS_UINT) {
            unsigned int num = jerry_get_number_value(value);
            sprintf(buf, "%u", num);
        } else if (type == IS_INT) {
            int num = jerry_get_number_value(value);
            sprintf(buf, "%d", num);
        }
    } else if (jerry_value_is_null(value)) {
        sprintf(buf, "null");
    }
    // NOTE: important that checks for function and array were above this
    else if (jerry_value_is_object(value)) {
        sprintf(buf, "[Object]");
    } else if (jerry_value_is_string(value)) {
        str_val = jerry_value_to_string(value);
        is_string = true;
    } else if (jerry_value_is_undefined(value)) {
        sprintf(buf, "undefined");
    } else {
        // should never get this
        sprintf(buf, "UNKNOWN");
    }

    if (is_string) {
        jerry_size_t size = jerry_get_string_size(str_val);
        if (size >= maxlen) {
            sprintf(buf, "[String - length %u]", (unsigned int)size);
        } else {
            char buffer[++size];
            zjs_copy_jstring(str_val, buffer, &size);
            if (quotes) {
                sprintf(buf, "\"%s\"", buffer);
            } else {
                sprintf(buf, "%s", buffer);
            }
        }
    }
    return true;
}

static void print_value(const jerry_value_t value, FILE *out, bool deep,
                        bool quotes)
{
    char buf[MAX_STR_LENGTH];
    if (!value2str(value, buf, MAX_STR_LENGTH, quotes) && deep) {
        if (jerry_value_is_array(value)) {
            u32_t len = jerry_get_array_length(value);
#ifdef JERRY_DEBUGGER
            jerry_debugger_send_output((jerry_char_t *)"[", 1, JERRY_DEBUGGER_OUTPUT_OK);
#endif
            fprintf(out, "[");
            for (int i = 0; i < len; i++) {
                if (i) {
#ifdef JERRY_DEBUGGER
                    jerry_debugger_send_output((jerry_char_t *)", ", 2, JERRY_DEBUGGER_OUTPUT_OK);
#endif
                    fprintf(out, ", ");
                }
                ZVAL element = jerry_get_property_by_index(value, i);
                print_value(element, out, false, true);
            }
#ifdef JERRY_DEBUGGER
            jerry_debugger_send_output((jerry_char_t *)"]", 1, JERRY_DEBUGGER_OUTPUT_OK);
#endif
            fprintf(out, "]");
        }
    } else {
#ifdef JERRY_DEBUGGER
        jerry_debugger_send_output((jerry_char_t *)buf, strlen(buf), JERRY_DEBUGGER_OUTPUT_OK);
#endif
        fprintf(out, "%s", buf);
    }
}

static ZJS_DECL_FUNC_ARGS(do_print, FILE *out)
{
    for (int i = 0; i < argc; i++) {
        if (i) {
            // insert spaces between arguments
#ifdef JERRY_DEBUGGER
            jerry_debugger_send_output((jerry_char_t *)" ", 1, JERRY_DEBUGGER_OUTPUT_OK);
#endif
            fprintf(out, " ");
        }
        print_value(argv[i], out, true, false);
    }
    fprintf(out, "\n");
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(console_log)
{
    return ZJS_CHAIN_FUNC_ARGS(do_print, stdout);
}

static ZJS_DECL_FUNC(console_error)
{
    return ZJS_CHAIN_FUNC_ARGS(do_print, stderr);
}

static ZJS_DECL_FUNC(console_time)
{
    // args: label
    ZJS_VALIDATE_ARGS(Z_STRING);

    u32_t start = zjs_port_timer_get_uptime();

    ZVAL num = jerry_create_number(start);
    jerry_set_property(gbl_time_obj, argv[0], num);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(console_time_end)
{
    // args: label
    ZJS_VALIDATE_ARGS(Z_STRING);

    ZVAL num = jerry_get_property(gbl_time_obj, argv[0]);
    jerry_delete_property(gbl_time_obj, argv[0]);

    if (!jerry_value_is_number(num)) {
        return TYPE_ERROR("unexpected value");
    }

    u32_t start = (u32_t)jerry_get_number_value(num);
    unsigned int milli = zjs_port_timer_get_uptime() - start;

    char *label = zjs_alloc_from_jstring(argv[0], NULL);
    const char *const_label = "unknown";
    if (label) {
        const_label = label;
    }

    // this print is part of the expected behavior for the user, don't remove
    ZJS_PRINT("%s: %ums\n", const_label, milli);
    zjs_free(label);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(console_assert)
{
    // args: validity[, output]
    ZJS_VALIDATE_ARGS(Z_BOOL, Z_OPTIONAL Z_ANY);

    char message[MAX_STR_LENGTH];
    bool b = jerry_get_boolean_value(argv[0]);
    if (!b) {
        if (argc > 1) {
            value2str(argv[1], message, MAX_STR_LENGTH, false);
            return zjs_custom_error("AssertionError", message, this,
                                    function_obj);
        } else {
            return zjs_custom_error("AssertionError", "console.assert", this,
                                    function_obj);
        }
    }
    return ZJS_UNDEFINED;
}

void zjs_console_init(void)
{
    ZVAL console = zjs_create_object();
    zjs_obj_add_function(console, "log", console_log);
    zjs_obj_add_function(console, "info", console_log);
    zjs_obj_add_function(console, "error", console_error);
    zjs_obj_add_function(console, "warn", console_error);
    zjs_obj_add_function(console, "time", console_time);
    zjs_obj_add_function(console, "timeEnd", console_time_end);
    zjs_obj_add_function(console, "assert", console_assert);

    ZVAL global_obj = jerry_get_global_object();
    zjs_set_property(global_obj, "console", console);

    // initialize the time object
    gbl_time_obj = zjs_create_object();
}

void zjs_console_cleanup()
{
    jerry_release_value(gbl_time_obj);
}

#endif  // BUILD_MODULE_CONSOLE
