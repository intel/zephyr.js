// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <string.h>

// ZJS includes
#include "zjs_util.h"

// fifo of pointers to zjs_callback objects representing JS callbacks
struct nano_fifo zjs_callbacks_fifo;

void zjs_queue_init()
{
    nano_fifo_init(&zjs_callbacks_fifo);
}

void zjs_queue_callback(struct zjs_callback *cb) {
    // requires: cb is a callback structure containing a pointer to a JS
    //             callback object and a wrapper function that knows how to
    //             call the callback; this structure may contain additional
    //             fields used by that wrapper; JS objects and values within
    //             the structure should already be ref-counted so they won't
    //             be lost, and you deref them from call_function
    //  effects: adds this callback info to a fifo queue and will call the
    //             wrapper with this structure later, in a safe way, within
    //             the task context for proper serialization
    nano_fifo_put(&zjs_callbacks_fifo, cb);
}

int count = 0;

void zjs_run_pending_callbacks()
{
    // requires: call only from task context
    //  effects: calls all the callbacks in the queue
    struct zjs_callback *cb;
    while (1) {
        cb = nano_task_fifo_get(&zjs_callbacks_fifo, TICKS_NONE);
        if (!cb) {
            count += 1;
            break;
        }
        count = 0;

        if (unlikely(!cb->call_function)) {
            PRINT("error: no JS callback found\n");
            continue;
        }

        cb->call_function(cb);
    }
}

void zjs_set_property(const jerry_value_t obj_val, const char *str_p,
                      const jerry_value_t prop_val)
{
    jerry_value_t name_val = jerry_create_string((jerry_char_t *)str_p);
    jerry_set_property(obj_val, name_val, prop_val);
    jerry_release_value(name_val);
}


jerry_value_t zjs_get_property (const jerry_value_t obj_val, const char *str_p)
{
    jerry_value_t prop_name_val = jerry_create_string ((const jerry_char_t *) str_p);
    jerry_value_t ret_val = jerry_get_property(obj_val, prop_name_val);
    jerry_release_value(prop_name_val);
    return ret_val;
}

void zjs_obj_add_boolean(jerry_value_t obj_val, bool bval,
                         const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_value_t name_val = jerry_create_string(name);
    jerry_value_t bool_val = jerry_create_boolean(bval);
    jerry_set_property(obj_val, name_val, bool_val);
    jerry_release_value(name_val);
    jerry_release_value(bool_val);
}

void zjs_obj_add_function(jerry_value_t obj_val, void *function,
                          const char *name)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function

    // NOTE: The docs on this function make it look like func obj should be
    //   released before we return, but in a loop of 25k buffer creates there
    //   seemed to be no memory leak. Reconsider with future intelligence.
    jerry_value_t name_val = jerry_create_string(name);
    jerry_value_t func_val = jerry_create_external_function(function);
    if (jerry_value_is_function(func_val)) {
        jerry_set_property(obj_val, name_val, func_val);
    }
    jerry_release_value(name_val);
    jerry_release_value(func_val);
}

void zjs_obj_add_object(jerry_value_t parent_val, jerry_value_t child_val,
                        const char *name)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    jerry_value_t name_val = jerry_create_string(name);
    jerry_set_property(parent_val, name_val, child_val);
    jerry_release_value(name_val);
}

void zjs_obj_add_string(jerry_value_t obj_val, const char *sval,
                        const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to sval
    jerry_value_t name_val = jerry_create_string(name);
    jerry_value_t str_val = jerry_create_string(sval);
    jerry_set_property(obj_val, name_val, str_val);
    jerry_release_value(name_val);
    jerry_release_value(str_val);
}

void zjs_obj_add_number(jerry_value_t obj_val, double nval, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to nval
    jerry_value_t name_val = jerry_create_string(name);
    jerry_value_t num_val = jerry_create_number(nval);
    jerry_set_property(obj_val, name_val, num_val);
    jerry_release_value(name_val);
    jerry_release_value(num_val);
}

bool zjs_obj_get_boolean(jerry_value_t obj_val, const char *name,
                         bool *flag)
{
    // requires: obj is an existing JS object, value name should exist as
    //             boolean
    //  effects: retrieves field specified by name as a boolean
    jerry_value_t value = zjs_get_property(obj_val, name);
    if (jerry_value_has_error_flag(value))
        return false;

    if (!jerry_value_is_boolean(value))
        return false;

    *flag = jerry_get_boolean_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_string(jerry_value_t obj_val, const char *name,
                        char *buffer, int len)
{
    // requires: obj is an existing JS object, value name should exist as
    //             string, buffer can receive the string, len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, copies at most len - 1 bytes plus a null terminator
    //             into buffer and returns true; otherwise, returns false
    jerry_value_t value = zjs_get_property(obj_val, name);
    if (jerry_value_has_error_flag(value))
        return false;

    if (!jerry_value_is_string(value))
        return false;

    jerry_size_t jlen = jerry_get_string_size(value);
    if (jlen >= len)
        jlen = len - 1;

    int wlen = jerry_string_to_char_buffer(value, buffer, jlen);

    buffer[wlen] = '\0';
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_double(jerry_value_t obj_val, const char *name,
                        double *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj_val, name);
    if (jerry_value_has_error_flag(value))
        return false;

    *num = jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_uint32(jerry_value_t obj_val, const char *name,
                        uint32_t *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj_val, name);
    if (jerry_value_has_error_flag(value))
        return false;

    *num = (uint32_t)jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_strequal(jerry_value_t str_val, const char *str) {
    // requires: jstr is a valid jerry string, str is a UTF-8 string
    //  effects: returns True if the strings are identical, false otherwise
    int len = strlen(str);

    jerry_size_t sz = jerry_get_string_size(str_val);
    if (len != sz)
        return false;

    jerry_char_t buf[len + 1];
    jerry_string_to_char_buffer(str_val, buf, sz);
    buf[len] = '\0';

    if (!strcmp(buf, str))
        return true;
    return false;
}

bool zjs_hex_to_byte(char *buf, uint8_t *byte)
{
    // requires: buf is a string with at least two hex chars
    //  effects: converts the first two hex chars in buf to a number and
    //             returns it in *byte; returns true on success, false otherwise
    uint8_t num = 0;
    for (int i=0; i<2; i++) {
        num <<= 4;
        if (buf[i] >= 'A' && buf[i] <= 'F')
            num += buf[i] - 'A' + 10;
        else if (buf[i] >= 'a' && buf[i] <= 'f')
            num += buf[i] - 'a' + 10;
        else if (buf[i] >= '0' && buf[i] <= '9')
            num += buf[i] - '0';
        else return false;
    }
    *byte = num;
    return true;
}

int zjs_identity(int num) {
    // effects: just returns the number passed to it; used as a default
    //            implementation for pin number conversions
    return num;
}

jerry_value_t zjs_error(const char *error)
{
    return jerry_create_error(JERRY_ERROR_TYPE, (jerry_char_t *)error);
}
