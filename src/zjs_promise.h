// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_promise_h__
#define __zjs_promise_h__

#include "jerryscript.h"

// ZJS includes
#include "zjs_common.h"

typedef struct zjs_promise {
    jerry_value_t promise_obj;
    jerry_value_t *argv;
    u32_t argc;
    struct zjs_promise *next;
} zjs_promise_t;

/*
* Creates a Promise and add references to the arguments
*
* @param argv          Arguments of the Promise being called
* @param argc          Number of arguments
*
* @return              Pointer to the struct associated with the Promise
*                        use this to resolve or reject the Promise
*/
zjs_promise_t *zjs_create_promise(const jerry_value_t argv[], u32_t argc);

/*
* Resolve a Promise
*
* @param p             Pointer to the struct associated with the Promise
* @param arg           Argument passed to the resolve function
* @param release       Promise will be released if it is set to true
*/
void zjs_resolve_promise(zjs_promise_t *p, jerry_value_t arg);

/*
* Rejects a Promise
*
* @param p             Pointer to the struct associated with the Promise
* @param error         Error passed to the reject function
* @param release       Promise will be released if it is set to true
*/
void zjs_reject_promise(zjs_promise_t *p, jerry_value_t error);

/** Initialize the promise module, or reinitialize after cleanup */
jerry_value_t zjs_promise_init();

/** Release resources held by the promise module */
void zjs_promise_cleanup();

#endif