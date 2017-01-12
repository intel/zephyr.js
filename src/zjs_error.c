// Copyright (c) 2016, Intel Corporation.

// ZJS includes
#include "zjs_util.h"

typedef struct {
    char *name;
    jerry_value_t ctor;
} zjs_error_t;

static zjs_error_t error_types[] = {
    { "SecurityError" },
    { "NotSupportedError" },
    { "SyntaxError" },
    { "TypeError" },
    { "RangeError" },
    { "TimeoutError" },
    { "NetworkError" },
    { "SystemError" }
};

static jerry_value_t error_handler(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    int count = sizeof(error_types) / sizeof(zjs_error_t);
    bool found = false;
    for (int i=0; i<count; i++) {
        if (function_obj == error_types[i].ctor) {
            zjs_obj_add_string(this, error_types[i].name, "name");
            found = true;
            break;
        }
    }

    // FIXME: may be able to remove because this should never happen
    if (!found) {
        return zjs_error("error_handler: unknown type");
    }

    if (argc >= 1 && jerry_value_is_string(argv[0])) {
        zjs_set_property(this, "message", argv[0]);
    }

    return ZJS_UNDEFINED;
}

void zjs_error_init()
{
    jerry_value_t global = jerry_get_global_object();
    jerry_value_t error_func = zjs_get_property(global, "Error");

    int count = sizeof(error_types) / sizeof(zjs_error_t);
    for (int i=0; i<count; i++) {
        jerry_value_t error_obj = jerry_construct_object(error_func, NULL, 0);
        if (jerry_value_is_undefined(error_obj)) {
            ERR_PRINT("Error object undefined\n");
        }

        // create a copy of this function w/ unique error prototype for each
        jerry_value_t ctor = jerry_create_external_function(error_handler);
        error_types[i].ctor = ctor;
        zjs_set_property(ctor, "prototype", error_obj);

        zjs_obj_add_object(global, ctor, error_types[i].name);

        jerry_release_value(error_obj);
    }

    jerry_release_value(error_func);
    jerry_release_value(global);
}

void zjs_error_cleanup()
{
    int count = sizeof(error_types) / sizeof(zjs_error_t);
    for (int i=0; i<count; i++) {
        jerry_release_value(error_types[i].ctor);
    }
}

jerry_value_t zjs_standard_error(zjs_error_type_t type, const char *message)
{
    int count = sizeof(error_types) / sizeof(zjs_error_t);
    if (type >= count || type < 0) {
#ifdef DEBUG_BUILD
        ZJS_PRINT("[Error] %s\n", message);
#endif
        return jerry_create_error(JERRY_ERROR_TYPE, (jerry_char_t *)message);
    }

#ifdef DEBUG_BUILD
    ZJS_PRINT("[%s] %s\n", error_types[type].name, message);
#endif
    jerry_value_t msg = jerry_create_string((jerry_char_t *)message);
    jerry_value_t obj = jerry_construct_object(error_types[type].ctor, &msg, 1);
    jerry_value_set_error_flag(&obj);

    jerry_release_value(msg);
    return obj;
}
