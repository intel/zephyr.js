// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_promises_h__
#define __zjs_promises_h__

#include "zjs_util.h"

/*
 * Function called after a promise has been fulfilled or rejected
 *
 * @param handle        Handle given to zjs_make_promise()
 */
typedef void (*zjs_post_promise_func)(void* handle);

/*
 * Turn an object into a promise
 *
 * @param obj           Object to make a promise
 * @param post          Function to be called when the promise has been fulfilled/rejected
 * @param handle        Handle passed to post function
 */
void zjs_make_promise(jerry_value_t obj, zjs_post_promise_func post,
                         void* handle);

/*
 * Fulfill a promise
 *
 * @param obj           Promise object
 * @param args          Array of args that will be given to then()
 * @param argc          Number of arguments in args
 */
void zjs_fulfill_promise(jerry_value_t obj, jerry_value_t args[], uint32_t argc);

/*
 * Reject a promise
 *
 * @param obj           Promise object
 * @param args          Array of args that will be given to catch()
 * @param argc          Number of arguments in args
 */
void zjs_reject_promise(jerry_value_t obj, jerry_value_t args[], uint32_t argc);

#endif /* __zjs_promises_h__ */
