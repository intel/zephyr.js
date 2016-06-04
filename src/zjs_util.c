// Copyright (c) 2016, Intel Corporation.

#include <string.h>

#include <zephyr.h>

#include "zjs_util.h"

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#define ZJS_CALLBACK_MAX_ARGS 3

struct zjs_callback_t {
    jerry_api_object_t *callback;
    jerry_api_value_t args[ZJS_CALLBACK_MAX_ARGS];
    jerry_api_length_t argc;
    struct zjs_callback_t *next;
};

struct zjs_callback_t *zjs_callbacks = NULL;

void zjs_queue_callback(jerry_api_object_t *callback,
                        jerry_api_value_t args_p[],
                        jerry_api_length_t args_cnt)
{
    // requires: callback is a JS function, args_p is an array of value args,
    //             args_cnt is the number of valid arg
    //  effects: allocates a callback queue item recording this callback
    //             and appends it to the list
    struct zjs_callback_t *cb = task_malloc(sizeof(struct zjs_callback_t));
    if (!cb) {
        PRINT("error: out of memory allocating callback struct\n");
        return;
    }

    if (args_cnt > ZJS_CALLBACK_MAX_ARGS) {
        PRINT("error: maximum callback args (%d) exceeded",
              ZJS_CALLBACK_MAX_ARGS);
        // try to call it anyway since it might handle undefined args
        args_cnt = ZJS_CALLBACK_MAX_ARGS;
    }

    cb->callback = jerry_api_acquire_object(callback);
    for (int i = 0; i < args_cnt; i++) {
        cb->args[i] = args_p[i];
        jerry_api_acquire_value(&cb->args[i]);
    }
    cb->argc = args_cnt;
    cb->next = NULL;

    // append callback at end of list
    struct zjs_callback_t **pcb = &zjs_callbacks;
    while (*pcb != NULL)
        pcb = &(*pcb)->next;
    *pcb = cb;
}

void zjs_run_pending_callbacks()
{
    // requires: running from a task
    //  effects: calls all the callbacks in the current list; any new callbacks
    //             added within these callbacks will be in a new queue and
    //             called later
    //  returns: true if there were callbacks to run, false otherwise
    struct zjs_callback_t *cb = zjs_callbacks;
    zjs_callbacks = NULL;

    if (!cb)
        return false;

    while (cb != NULL) {
        jerry_api_value_t rval;
        if (jerry_api_call_function(cb->callback, NULL, &rval, cb->args,
                                    cb->argc))
            jerry_api_release_value(&rval);

        jerry_api_release_object(cb->callback);
        for (int i = 0; i < cb->argc; i++)
            jerry_api_release_value(&cb->args[i]);

        struct zjs_callback_t *next = cb->next;
        task_free(cb);
        cb = next;
    }

    return true;
}

void zjs_obj_add_boolean(jerry_api_object_t *obj, bool bval, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_api_value_t value = jerry_api_create_boolean_value(bval);
    jerry_api_set_object_field_value(obj, (const jerry_api_char_t *)name,
                                     &value);
}

void zjs_obj_add_function(jerry_api_object_t *obj, void *function,
                          const char *name)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function
    jerry_api_object_t *func = jerry_api_create_external_function(function);
    jerry_api_value_t value;
    value.type = JERRY_API_DATA_TYPE_OBJECT;
    value.u.v_object = func;
    jerry_api_set_object_field_value(obj, (const jerry_api_char_t *)name,
                                     &value);
}

void zjs_obj_add_object(jerry_api_object_t *parent, jerry_api_object_t *child,
                        const char *name)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    jerry_api_value_t value;
    value.type = JERRY_API_DATA_TYPE_OBJECT;
    value.u.v_object = child;
    jerry_api_set_object_field_value(parent, (const jerry_api_char_t *)name,
                                     &value);
}

void zjs_obj_add_string(jerry_api_object_t *obj, const char *sval,
                        const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_api_string_t *str = jerry_api_create_string(sval);
    jerry_api_value_t value = jerry_api_create_string_value(str);
    jerry_api_set_object_field_value(obj, name, &value);
}

void zjs_obj_add_uint32(jerry_api_object_t *obj, uint32_t ival,
                        const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_api_value_t value;
    value.type = JERRY_API_DATA_TYPE_UINT32;
    value.u.v_uint32 = ival;
    jerry_api_set_object_field_value(obj, name, &value);
}

bool zjs_obj_get_boolean(jerry_api_object_t *obj, const char *name,
                         bool *flag)
{
    // requires: obj is an existing JS object, value name should exist as
    //             boolean
    //  effects: retrieves field specified by name as a boolean
    jerry_api_value_t value;
    if (!jerry_api_get_object_field_value(obj, name, &value))
        return false;

    if (value.type != JERRY_API_DATA_TYPE_BOOLEAN)
        return false;

    *flag = jerry_api_get_boolean_value(&value);
    jerry_api_release_value(&value);
    return true;
}

bool zjs_obj_get_string(jerry_api_object_t *obj, const char *name,
                        char *buffer, int len)
{
    // requires: obj is an existing JS object, value name should exist as
    //             string, buffer can receive the string, len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, copies at most len - 1 bytes plus a null terminator
    //             into buffer and returns true; otherwise, returns false
    jerry_api_value_t value;
    if (!jerry_api_get_object_field_value(obj, name, &value))
        return false;

    if (value.type != JERRY_API_DATA_TYPE_STRING)
        return false;

    jerry_api_string_t *str = jerry_api_get_string_value(&value);
    jerry_api_size_t jlen = jerry_api_get_string_size(str);
    if (jlen + 1 < len)
        len = jlen + 1;

    int wlen = jerry_api_string_to_char_buffer(str, buffer, len);
    buffer[wlen] = '\0';
    jerry_api_release_value(&value);
    return true;
}

bool zjs_obj_get_uint32(jerry_api_object_t *obj, const char *name,
                        uint32_t *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_api_value_t value;
    if (!jerry_api_get_object_field_value(obj, name, &value))
        return false;

    // work around bug in the above API, it never returns false
    if (value.type == JERRY_API_DATA_TYPE_UNDEFINED)
        return false;

    *num = (uint32_t)jerry_api_get_number_value(&value);
    jerry_api_release_value(&value);
    return true;
}

// FIXME: NOT BEING USED CURRENTLY
// zjs_obj_get_string + strcmp suffices for my current needs, although I had
// already debugged this function so I want to at least check it in and see if
// it becomes useful
bool zjs_strequal(const jerry_api_string_t *jstr, const char *str)
{
    // requires: str is null-terminated and should be small so we don't use up
    //             too much stack space
    //  effects: returns the results of strcmp between the string underlying
    //             jstr and str
    int len = strlen(str);
    jerry_api_size_t jlen = jerry_api_get_string_size(jstr);
    if (len != jlen)
        return false;

    char buffer[jlen];
    int wlen = jerry_api_string_to_char_buffer(jstr, buffer, jlen);
    buffer[wlen] = '\0';
    for (int i=0; i<len; i++) {
        if (buffer[i] != str[i])
            return false;
    }
    return true;
}

/**
 * Initialize Jerry API value with specified object
 */
void zjs_init_api_value_object (jerry_api_value_t *out_value_p, /**< out: API value */
                                jerry_api_object_t *v) /**< object value to initialize with */
{
    // requires: out_value_p to recieve the object value v
    //  effects: put the object into out_value_p with appropriate encoding.
    jerry_api_acquire_object (v);

    out_value_p->type = JERRY_API_DATA_TYPE_OBJECT;
    out_value_p->u.v_object = v;
}

/**
 * Initialize Jerry API value with specified string
 */
void zjs_init_api_value_string (jerry_api_value_t *out_value_p, /**< out: API value */
                                const char *v) /**< string value to initialize with */
{
    // requires: out_value_p to recieve the string value v
    //  effects: put the string into out_value_p with appropriate encoding.

    out_value_p->type = JERRY_API_DATA_TYPE_STRING;
    out_value_p->u.v_string = jerry_api_create_string ((jerry_api_char_t *) v);
}
