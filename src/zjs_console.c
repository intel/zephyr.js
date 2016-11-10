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

int is_int(jerry_value_t val) {
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

static jerry_value_t do_print(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc,
                              FILE* out)
{
    int i;
    for (i = 0; i < argc; ++i) {
        if (jerry_value_is_string(argv[i])) {
            jerry_size_t jlen = jerry_get_string_size(argv[0]);
            if (jlen > MAX_STR_LENGTH) {
                ERR_PRINT("console string is too long, max length=%u\n", MAX_STR_LENGTH);
                return ZJS_UNDEFINED;
            }
            char buffer[jlen + 1];
            int wlen = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)buffer, jlen);
            buffer[wlen] = '\0';
            fprintf(out, "%s ", buffer);
        } else if (jerry_value_is_number(argv[i])) {
            int type = is_int(argv[i]);
            if (type == IS_NUMBER) {
                double num = jerry_get_number_value(argv[i]);
                fprintf(out, "%f ", num);
            } else if (type == IS_UINT) {
                uint32_t num = (uint32_t)jerry_get_number_value(argv[i]);
                fprintf(out, "%lu ", num);
            } else if (type == IS_INT) {
                int32_t num = (int32_t)jerry_get_number_value(argv[i]);
                // Linux and Zephyr print int32_t's differently if %li is used
#ifdef ZJS_LINUX_BUILD
                fprintf(out, "%i ", num);
#else
                fprintf(out, "%li ", num);
#endif
            }
        } else if (jerry_value_is_boolean(argv[i])) {
            uint8_t val = jerry_get_boolean_value(argv[i]);
            fprintf(out, (val) ? "true " : "false ");
        } else if (jerry_value_is_object(argv[i])) {
            fprintf(out, "[Object] ");
        }
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
