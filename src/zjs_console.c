// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_CONSOLE

#include "zjs_common.h"
#include "zjs_util.h"

#define MAX_STR_LENGTH   256

#ifdef ZJS_LINUX_BUILD
#define STDERR_PRINT(s, ...) fprintf(stderr, s, __VA_ARGS__)
#define STDOUT_PRINT(s, ...) fprintf(stdout, s, __VA_ARGS__)
#else
#define STDERR_PRINT(s, ...) PRINT(s, __VA_ARGS__)
#define STDOUT_PRINT(s, ...) PRINT(s, __VA_ARGS__)
#endif

static jerry_value_t console_log(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be a string\n");
        return ZJS_UNDEFINED;
    }
    jerry_size_t jlen = jerry_get_string_size(argv[0]);
    if (jlen > MAX_STR_LENGTH) {
        ERR_PRINT("console string is too long, max length=%u\n", MAX_STR_LENGTH);
        return ZJS_UNDEFINED;
    }
    char buffer[jlen + 1];
    int wlen = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)buffer, jlen);
    buffer[wlen] = '\0';

    STDOUT_PRINT("%s\n", buffer);

    return ZJS_UNDEFINED;
}

static jerry_value_t console_error(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("first parameter must be a string\n");
        return ZJS_UNDEFINED;
    }
    jerry_size_t jlen = jerry_get_string_size(argv[0]);
    if (jlen > MAX_STR_LENGTH) {
        ERR_PRINT("console string is too long, max length=%u\n", MAX_STR_LENGTH);
        return ZJS_UNDEFINED;
    }
    char buffer[jlen + 1];
    int wlen = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)buffer, jlen);
    buffer[wlen] = '\0';

    STDERR_PRINT("%s\n", buffer);

    return ZJS_UNDEFINED;
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
