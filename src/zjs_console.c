// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_CONSOLE

#include "zjs_common.h"
#include "zjs_util.h"

#define MAX_STR_LENGTH   256

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

static int is_int(jerry_value_t val) {
    int ret = 0;
    double n = jerry_get_number_value(val);
    ret = (n - (int) n == 0);
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

static void print_value(const jerry_value_t value, FILE *out, bool deep,
                        bool quotes)
{
    if (jerry_value_is_array(value)) {
        uint32_t len = jerry_get_array_length(value);
        if (deep) {
            fprintf(out, "[");
            for (int i = 0; i < len; i++) {
                if (i) {
                    fprintf(out, ", ");
                }
                jerry_value_t element = jerry_get_property_by_index(value, i);
                print_value(element, out, false, true);
            }
            fprintf(out, "]");
        }
        else {
            fprintf(out, "[Array - length %lu]", len);
        }
    }
    else if (jerry_value_is_boolean(value)) {
        uint8_t val = jerry_get_boolean_value(value);
        fprintf(out, (val) ? "true" : "false");
    }
    else if (jerry_value_is_function(value)) {
        fprintf(out, "[Function]");
    }
    else if (jerry_value_is_number(value)) {
        int type = is_int(value);
        if (type == IS_NUMBER) {
#ifdef ZJS_PRINT_FLOATS
            double num = jerry_get_number_value(value);
            fprintf(out, "%f", num);
#else
            int32_t num = (int32_t)jerry_get_number_value(value);
            fprintf(out, "[Float ~%li]", num);
#endif
        } else if (type == IS_UINT) {
            uint32_t num = (uint32_t)jerry_get_number_value(value);
            fprintf(out, "%lu", num);
        } else if (type == IS_INT) {
            int32_t num = (int32_t)jerry_get_number_value(value);
            // Linux and Zephyr print int32_t's differently if %li is used
#ifdef ZJS_LINUX_BUILD
            fprintf(out, "%i", num);
#else
            fprintf(out, "%li", num);
#endif
        }
    }
    else if (jerry_value_is_null(value)) {
        fprintf(out, "null");
    }
    // NOTE: important that checks for function and array were above this
    else if (jerry_value_is_object(value)) {
        fprintf(out, "[Object]");
    }
    else if (jerry_value_is_string(value)) {
        jerry_size_t jlen = jerry_get_string_size(value);
        if (jlen > MAX_STR_LENGTH) {
            fprintf(out, "[String - length %lu]", jlen);
        }
        else {
            char buffer[jlen + 1];
            int wlen = jerry_string_to_char_buffer(value,
                                                   (jerry_char_t *)buffer,
                                                   jlen);
            buffer[wlen] = '\0';
            if (quotes) {
                fprintf(out, "\"%s\"", buffer);
            }
            else {
                fprintf(out, "%s", buffer);
            }
        }
    }
    else if (jerry_value_is_undefined(value)) {
        fprintf(out, "undefined");
    }
    else {
        // should never get this
        fprintf(out, "UNKNOWN");
    }
}

static jerry_value_t do_print(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc,
                              FILE* out)
{
    for (int i = 0; i < argc; i++) {
        if (i) {
            // insert spaces between arguments
            fprintf(out, " ");
        }
        print_value(argv[i], out, true, false);
    }
    fprintf(out, "\n");
    return ZJS_UNDEFINED;
}

static jerry_value_t console_log(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    return do_print(function_obj, this, argv, argc, stdout);
}

static jerry_value_t console_error(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    return do_print(function_obj, this, argv, argc, stderr);
}

void zjs_console_init(void)
{
    jerry_value_t global_obj = jerry_get_global_object();

    jerry_value_t console = jerry_create_object();
    zjs_obj_add_function(console, console_log, "log");
    zjs_obj_add_function(console, console_log, "info");
    zjs_obj_add_function(console, console_error, "error");
    zjs_obj_add_function(console, console_error, "warn");

    zjs_set_property(global_obj, "console", console);
    jerry_release_value(global_obj);
}

#endif
