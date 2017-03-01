// Copyright (c) 2016-2017, Intel Corporation.

#include <string.h>
#include "zjs_util.h"
#include "zjs_common.h"
#include "zjs_promise.h"
#include "zjs_callbacks.h"

typedef struct zjs_promise {
    zjs_callback_id then_id;    // Callback ID for then JS callback
    zjs_callback_id catch_id;   // Callback ID for catch JS callback
    void* user_handle;
    zjs_post_promise_func post;
} zjs_promise_t;

zjs_promise_t *new_promise(void)
{
    zjs_promise_t *new = zjs_malloc(sizeof(zjs_promise_t));
    memset(new, 0, sizeof(zjs_promise_t));
    new->catch_id = -1;
    new->then_id = -1;
    return new;
}

static void post_promise(void* h, jerry_value_t* ret_val)
{
    zjs_promise_t *handle = (zjs_promise_t *)h;
    if (handle) {
        if (handle->post) {
            handle->post(handle->user_handle);
        }
        zjs_free(handle);
    }
}

static jerry_value_t promise_resolve(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc,
                                     uint8_t fulfill)
{
    if (!jerry_value_is_function(argv[0])) {
        return zjs_error("invalid arguments");
    }

    zjs_promise_t *handle = NULL;
    jerry_value_t promise_obj = zjs_get_property(this, "promise");
    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);
    jerry_release_value(promise_obj);

    if (!handle) {
        return zjs_error("promise handle not found");
    }
    // we don't need to check the return of edit because either fulfill or
    // reject will have removed then_id or catch_id callback
    if (fulfill) {
        zjs_edit_js_func(handle->then_id, argv[0]);
    } else {
        zjs_edit_js_func(handle->catch_id, argv[0]);
    }

    return jerry_acquire_value(this);
}

static jerry_value_t promise_then(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    return promise_resolve(function_obj, this, argv, argc, 1);
}

static jerry_value_t promise_catch(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    return promise_resolve(function_obj, this, argv, argc, 0);
}

void zjs_make_promise(jerry_value_t obj, zjs_post_promise_func post,
                      void* handle)
{
    zjs_promise_t *new = new_promise();
    if (!new) {
        ERR_PRINT("could not allocate new promise\n");
        return;
    }
    jerry_value_t promise_obj = jerry_create_object();

    zjs_obj_add_function(obj, promise_then, "then");
    zjs_obj_add_function(obj, promise_catch, "catch");

    jerry_set_object_native_handle(promise_obj, (uintptr_t)new, NULL);

    new->user_handle = handle;
    new->post = post;

    new->then_id = zjs_add_callback_once(ZJS_UNDEFINED,
                                         obj,
                                         handle,
                                         post_promise);

    new->catch_id = zjs_add_callback_once(ZJS_UNDEFINED,
                                          obj,
                                          handle,
                                          post_promise);

    // Add the "promise" object to the object passed as a property, because the
    // object being made to a promise may already have a native handle.
    zjs_obj_add_object(obj, promise_obj, "promise");
    jerry_release_value(promise_obj);

    DBG_PRINT("created promise, obj=%lu, promise=%p, handle=%p\n", obj, new,
              handle);
}

void zjs_fulfill_promise(jerry_value_t obj, jerry_value_t argv[], uint32_t argc)
{
    zjs_promise_t *handle = NULL;
    jerry_value_t promise_obj = zjs_get_property(obj, "promise");

    if (!jerry_value_is_object(promise_obj)) {
        jerry_release_value(promise_obj);
        ERR_PRINT("'promise' not found in object %lu\n", obj);
        return;
    }

    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);
    jerry_release_value(promise_obj);
    if (handle) {
        zjs_signal_callback(handle->then_id, argv,
                            argc * sizeof(jerry_value_t));
        zjs_remove_callback(handle->catch_id);
        DBG_PRINT("fulfilling promise, obj=%lu, then_id=%d, argv=%p, nargs=%lu\n",
                  obj, handle->then_id, argv, argc);
    } else {
        ERR_PRINT("native handle not found\n");
    }
}

void zjs_reject_promise(jerry_value_t obj, jerry_value_t argv[], uint32_t argc)
{
    zjs_promise_t *handle = NULL;
    jerry_value_t promise_obj = zjs_get_property(obj, "promise");

    if (!jerry_value_is_object(promise_obj)) {
        jerry_release_value(promise_obj);
        ERR_PRINT("'promise' not found in object %lu\n", obj);
        return;
    }

    jerry_get_object_native_handle(promise_obj, (uintptr_t*)&handle);
    jerry_release_value(promise_obj);
    if (handle) {
        zjs_signal_callback(handle->catch_id, argv,
                            argc * sizeof(jerry_value_t));
        zjs_remove_callback(handle->then_id);
        DBG_PRINT("rejecting promise, obj=%lu, catch_id=%d, argv=%p, nargs=%lu\n",
                  obj, handle->catch_id, argv, argc);
    } else {
        ERR_PRINT("native handle not found\n");
    }
}
