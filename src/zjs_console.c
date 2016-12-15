// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_CONSOLE

#include "zjs_common.h"
#include "zjs_util.h"
#ifdef ZJS_LINUX_BUILD
#include "zjs_linux_port.h"
#else
#include "zjs_zephyr_port.h"
#endif

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

static jerry_value_t gbl_time_obj;
static jerry_value_t console_prototype_obj;

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
        jerry_size_t size = jerry_get_string_size(value);
        if (size >= MAX_STR_LENGTH) {
            fprintf(out, "[String - length %lu]", size);
        }
        else {
            char buffer[++size];
            zjs_copy_jstring(value, buffer, &size);
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

static jerry_value_t console_time(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("invalid parameters\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char label[sz + 1];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)label, sz);
    if (sz != len) {
        ERR_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    label[len] = '\0';

    jerry_value_t new_timer_obj = jerry_create_object();
    zjs_port_timer_t* timer = zjs_malloc(sizeof(zjs_port_timer_t));
    if (!timer) {
        ERR_PRINT("could not allocate timer object\n");
        return ZJS_UNDEFINED;
    }
    zjs_port_timer_init(timer);
    // set timer to go off every millisecond (this is the best accuracy we can get)
    zjs_port_timer_start(timer, 1);

    jerry_set_object_native_handle(new_timer_obj, (uintptr_t)timer, NULL);
    // add the new timer to the global timer object
    zjs_set_property(gbl_time_obj, label, new_timer_obj);

    return ZJS_UNDEFINED;
}

static jerry_value_t console_time_end(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        ERR_PRINT("invalid parameters\n");
        return ZJS_UNDEFINED;
    }
    int sz = jerry_get_string_size(argv[0]);
    char label[sz + 1];
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)label, sz);
    if (sz != len) {
        ERR_PRINT("size mismatch\n");
        return ZJS_UNDEFINED;
    }
    label[len] = '\0';

    jerry_value_t timer_obj = zjs_get_property(gbl_time_obj, label);
    if (!jerry_value_is_object(timer_obj)) {
        ERR_PRINT("timer %s not found\n", label);
        return ZJS_UNDEFINED;
    }

    zjs_port_timer_t* timer;

    if (!jerry_get_object_native_handle(timer_obj, (uintptr_t*)&timer)) {
        ERR_PRINT("native timer handle not found\n");
        return ZJS_UNDEFINED;
    }

    uint32_t milli = zjs_port_timer_test(timer) * 10;
    zjs_port_timer_stop(timer);

    zjs_free(timer);

    fprintf(stdout, "%s:%lums\n", label, milli);

    return ZJS_UNDEFINED;
}

static jerry_value_t get_console_obj(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    if (!jerry_value_is_object(console_prototype_obj)) {
        ERR_PRINT("console not initialized\n");
        return ZJS_UNDEFINED;
    }
    jerry_value_t console = jerry_create_object();
    jerry_set_prototype(console, console_prototype_obj);

    return console;
}

static jerry_value_t console_assert(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    char message[128];
    uint8_t has_message = 0;
    if (!jerry_value_is_boolean(argv[0])) {
        ERR_PRINT("invalid parameters\n");
        return ZJS_UNDEFINED;
    }
    bool b = jerry_get_boolean_value(argv[0]);
    if (!b) {
        if (argc > 1) {
            if (!jerry_value_is_string(argv[1])) {
                ERR_PRINT("message parameter must be a string\n");
                return ZJS_UNDEFINED;
            } else {
                int sz = jerry_get_string_size(argv[1]);
                if (sz > 128) {
                    ERR_PRINT("error message too long\n");
                    return ZJS_UNDEFINED;
                }
                int len = jerry_string_to_char_buffer(argv[1], (jerry_char_t *)message, sz);
                if (sz != len) {
                    ERR_PRINT("size mismatch\n");
                    return ZJS_UNDEFINED;
                }
                message[len] = '\0';
                has_message = 1;
            }
        }
        if (has_message) {
            return zjs_error(message);
        } else {
            return zjs_error("console.assert() error");
        }
    }
    return ZJS_UNDEFINED;
}

void zjs_console_init(void)
{
    jerry_value_t global_obj = jerry_get_global_object();

    jerry_value_t console = jerry_create_object();

    zjs_obj_add_function(global_obj, get_console_obj, "Console");

    zjs_native_func_t array[] = {
        { console_log, "log" },
        { console_log, "info" },
        { console_error, "error" },
        { console_error, "warn" },
        { console_time, "time" },
        { console_time_end, "timeEnd" },
        { console_assert, "assert" },
        { NULL, NULL }
    };

    console_prototype_obj = jerry_create_object();
    zjs_obj_add_functions(console_prototype_obj, array);

    jerry_set_prototype(console, console_prototype_obj);

    zjs_set_property(global_obj, "console", console);
    jerry_release_value(global_obj);

    // initialize the time object
    gbl_time_obj = jerry_create_object();
}

#endif
