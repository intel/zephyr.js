// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_util.h"

typedef struct {
    char *name;
    jerry_value_t ctor;
} zjs_error_t;

static zjs_error_t error_types[] = {
    { "NetworkError" },
    { "NotSupportedError" },
    { "RangeError" },
    { "SecurityError" },
    { "SyntaxError" },
    { "SystemError" },
    { "TimeoutError" },
    { "TypeError" },
};

static ZJS_DECL_FUNC(error_handler)
{
    // args: message
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_STRING);

    int count = sizeof(error_types) / sizeof(zjs_error_t);
    bool found = false;
    for (int i = 0; i < count; i++) {
        if (function_obj == error_types[i].ctor) {
            zjs_obj_add_string(this, error_types[i].name, "name");
            found = true;
            break;
        }
    }

    // FIXME: may be able to remove because this should never happen
    if (!found) {
        return zjs_error("unknown type");
    }

    if (argc >= 1) {
        zjs_set_property(this, "message", argv[0]);
    }

    return ZJS_UNDEFINED;
}

void zjs_error_init()
{
    ZVAL global = jerry_get_global_object();
    ZVAL error_func = zjs_get_property(global, "Error");

    int count = sizeof(error_types) / sizeof(zjs_error_t);
    for (int i = 0; i < count; i++) {
        ZVAL error_obj = jerry_construct_object(error_func, NULL, 0);
        if (jerry_value_is_undefined(error_obj)) {
            ERR_PRINT("Error object undefined\n");
        }

        // create a copy of this function w/ unique error prototype for each
        ZVAL ctor = jerry_create_external_function(error_handler);
        error_types[i].ctor = ctor;
        zjs_set_property(ctor, "prototype", error_obj);

        zjs_obj_add_object(global, ctor, error_types[i].name);
    }
}

void zjs_error_cleanup()
{
    int count = sizeof(error_types) / sizeof(zjs_error_t);
    for (int i = 0; i < count; i++) {
        jerry_release_value(error_types[i].ctor);
    }
}

static char *construct_message(jerry_value_t this, jerry_value_t func,
                               const char *message)
{
    jerry_size_t size = 64;
    char name[size];
    ZVAL keys_array = jerry_get_object_keys(this);
    if (!jerry_value_is_array(keys_array)) {
        return NULL;
    }
    u32_t arr_length = jerry_get_array_length(keys_array);
    int i;
    for (i = 0; i < arr_length; ++i) {
        ZVAL val = jerry_get_property_by_index(keys_array, i);
        zjs_copy_jstring(val, name, &size);
        ZVAL func_val = zjs_get_property(this, name);
        if (func_val == func) {
            break;
        }
    }

    int mlen = strlen(message);
    char *msg = zjs_malloc(mlen + size + 4);

    memcpy(msg, name, size);
    msg[size] = '(';
    msg[size + 1] = ')';
    msg[size + 2] = ':';
    memcpy(msg + size + 3, message, mlen);
    msg[mlen + size + 3] = '\0';

    return msg;
}

jerry_value_t zjs_custom_error_with_func(jerry_value_t this,
                                         jerry_value_t func,
                                         const char *name,
                                         const char *message)
{
    jerry_value_t error;
    char *msg = construct_message(this, func, message);
    if (!msg) {
        // Problem finding function
        DBG_PRINT("calling function not found\n");
        error = zjs_custom_error(name, message, this, func);
    } else {
        error = zjs_custom_error(name, msg, this, func);
        zjs_free(msg);
    }
    return error;
}

jerry_value_t zjs_error_with_func(jerry_value_t this,
                                  jerry_value_t func,
                                  zjs_error_type_t type,
                                  const char *message)

{
    jerry_value_t error;
    char *msg = construct_message(this, func, message);
    if (!msg) {
        // Problem finding function
        DBG_PRINT("calling function not found\n");
        error = zjs_standard_error(type, message, this, func);
    } else {
        error = zjs_standard_error(type, msg, this, func);
        zjs_free(msg);
    }
    return error;
}

static void add_context(jerry_value_t error, jerry_value_t this,
                        jerry_value_t func)
{
    // FIXME: move this to a native pointer after the new jerryscript goes in
    if (jerry_value_is_object(func)) {
        zjs_obj_add_object(error, func, "function");
        if (jerry_value_is_object(this)) {
            zjs_obj_add_object(error, this, "this");
        }
    }
}

jerry_value_t zjs_custom_error(const char *name, const char *message,
                               jerry_value_t this, jerry_value_t func)
{
    jerry_value_t error = jerry_create_error(JERRY_ERROR_TYPE,
                                             (jerry_char_t *)message);
    zjs_obj_add_string(error, name, "name");
    add_context(error, this, func);
    return error;
}

jerry_value_t zjs_standard_error(zjs_error_type_t type, const char *message,
                                 jerry_value_t this, jerry_value_t func)
{
    int count = sizeof(error_types) / sizeof(zjs_error_t);
    jerry_value_t error;
    if ((unsigned int)type >= count) {
#ifdef DEBUG_BUILD
        ZJS_PRINT("[Error] %s\n", message);
#endif
        error = jerry_create_error(JERRY_ERROR_TYPE, (jerry_char_t *)message);
    } else {
#ifdef DEBUG_BUILD
        ZJS_PRINT("[%s] %s\n", error_types[type].name, message);
#endif
        ZVAL msg = jerry_create_string((jerry_char_t *)message);
        error = jerry_construct_object(error_types[type].ctor, &msg, 1);
        jerry_value_set_error_flag(&error);
    }

    add_context(error, this, func);
    return error;
}
