// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_TEST_PROMISE

#include "zjs_common.h"
#include "zjs_callbacks.h"
#include "zjs_promise.h"
#include "zjs_util.h"

typedef struct dummy_handle {
    uint32_t value;
} dummy_handle_t;

#define TEST_VAL       42

static void post_promise(void *handle)
{
    if (handle) {
        dummy_handle_t *h = (dummy_handle_t *)handle;
        if (h->value != TEST_VAL) {
            ERR_PRINT("Handle was not valid!!!\n");
        }
        zjs_free(h);
    }
}

static jerry_value_t create_promise(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    dummy_handle_t *handle = zjs_malloc(sizeof(dummy_handle_t));
    handle->value = TEST_VAL;

    jerry_value_t promise = jerry_create_object();
    ZJS_PRINT("Testing promise, object = %u\n", promise);

    zjs_make_promise(promise, post_promise, handle);

    return promise;
}

static jerry_value_t test_fulfill(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    // args: promise object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    zjs_fulfill_promise(argv[0], NULL, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t test_reject(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc)
{
    // args: promise object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    zjs_reject_promise(argv[0], NULL, 0);
    return ZJS_UNDEFINED;
}


jerry_value_t zjs_test_promise_init(void)
{
    jerry_value_t test = jerry_create_object();
    zjs_obj_add_function(test, create_promise, "create_promise");
    zjs_obj_add_function(test, test_fulfill, "fulfill");
    zjs_obj_add_function(test, test_reject, "reject");

    return test;
}

void zjs_test_promise_cleanup()
{
}

#endif  // BUILD_MODULE_CONSOLE
