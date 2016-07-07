// Copyright (c) 2016, Intel Corporation.

#include <string.h>

#include <zephyr.h>

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

void zjs_obj_add_boolean(jerry_object_t *obj, bool bval, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_value_t value = jerry_create_boolean_value(bval);
    jerry_set_object_field_value(obj, (const jerry_char_t *)name, value);
}

void zjs_obj_add_function(jerry_object_t *obj, void *function,
                          const char *name)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function

    // NOTE: The docs on this function make it look like func obj should be
    //   released before we return, but in a loop of 25k buffer creates there
    //   seemed to be no memory leak. Reconsider with future intelligence.
    jerry_object_t *func = jerry_create_external_function(function);
    jerry_value_t value = jerry_create_object_value(func);
    jerry_set_object_field_value(obj, (const jerry_char_t *)name, value);
    jerry_release_value(value);
}

void zjs_obj_add_object(jerry_object_t *parent, jerry_object_t *child,
                        const char *name)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    jerry_value_t value = jerry_create_object_value(child);
    jerry_set_object_field_value(parent, (const jerry_char_t *)name, value);
    jerry_release_value(value);
}

void zjs_obj_add_string(jerry_object_t *obj, const char *sval,
                        const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to sval
    jerry_string_t *str = jerry_create_string(sval);
    jerry_value_t value = jerry_create_string_value(str);
    jerry_set_object_field_value(obj, name, value);
    jerry_release_value(value);
}

void zjs_obj_add_number(jerry_object_t *obj, double nval, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to nval
    jerry_value_t value = jerry_create_number_value(nval);
    jerry_set_object_field_value(obj, name, value);
}

bool zjs_obj_get_boolean(jerry_object_t *obj, const char *name,
                         bool *flag)
{
    // requires: obj is an existing JS object, value name should exist as
    //             boolean
    //  effects: retrieves field specified by name as a boolean
    jerry_value_t value = jerry_get_object_field_value(obj, name);
    if (jerry_value_is_error(value))
        return false;

    if (!jerry_value_is_boolean(value))
        return false;

    *flag = jerry_get_boolean_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_string(jerry_object_t *obj, const char *name,
                        char *buffer, int len)
{
    // requires: obj is an existing JS object, value name should exist as
    //             string, buffer can receive the string, len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, copies at most len - 1 bytes plus a null terminator
    //             into buffer and returns true; otherwise, returns false
    jerry_value_t value = jerry_get_object_field_value(obj, name);
    if (jerry_value_is_error(value))
        return false;

    if (!jerry_value_is_string(value))
        return false;

    jerry_string_t *str = jerry_get_string_value(value);
    jerry_size_t jlen = jerry_get_string_size(str);
    if (jlen + 1 < len)
        len = jlen + 1;

    int wlen = jerry_string_to_char_buffer(str, buffer, len);
    buffer[wlen] = '\0';
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_double(jerry_object_t *obj, const char *name,
                        double *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = jerry_get_object_field_value(obj, name);
    if (jerry_value_is_error(value))
        return false;

    *num = jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_uint32(jerry_object_t *obj, const char *name,
                        uint32_t *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = jerry_get_object_field_value(obj, name);
    if (jerry_value_is_error(value))
        return false;

    *num = (uint32_t)jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

/**
 * Initialize Jerry value with specified object
 */
void zjs_init_value_object(jerry_value_t *out_value_p, jerry_object_t *v)
{
    // requires: out_value_p to receive the object value v
    //  effects: put the object into out_value_p with appropriate encoding.
    jerry_acquire_object(v);
    *out_value_p = jerry_create_object_value(v);
}

/**
 * Initialize Jerry value with specified string
 */
void zjs_init_value_string(jerry_value_t *out_value_p, const char *v)
{
    // requires: out_value_p to receive the string value v
    //  effects: put the string into out_value_p with appropriate encoding.
    *out_value_p = jerry_create_string_value(jerry_create_string((jerry_char_t *) v));
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
