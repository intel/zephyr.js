// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

// ZJS includes
#include "zjs_promise.h"
#include "zjs_util.h"

// JerryScript includes
#include "jerryscript.h"

static zjs_promise_t *zjs_promises = NULL;

zjs_promise_t *zjs_create_promise(const jerry_value_t argv[], u32_t argc)
{
    zjs_promise_t *p = zjs_malloc(sizeof(zjs_promise_t));

    if (!p) {
        ERR_PRINT("out of memory allocating promise struct\n");
        return NULL;
    }
    memset(p, 0, sizeof(zjs_promise_t));

    p->argc = argc;
    p->promise_obj = jerry_create_promise();

    if (p->argc > 0) {
        p->argv = zjs_malloc(sizeof(jerry_value_t) * argc);

        if (!p->argv) {
            jerry_release_value(p->promise_obj);
            zjs_free(p);
            ERR_PRINT("out of memory allocating promise args\n");
            return NULL;
        }
        for (int i = 0; i < argc; ++i) {
            p->argv[i] = jerry_acquire_value(argv[i]);
        }
    } else {
        p->argv = NULL;
    }

    ZJS_LIST_APPEND(zjs_promise_t, zjs_promises, p);
    DBG_PRINT("added promise, p=%p, argv=%p, argc=%u\n",
              p, argv, argc);
    return p;
}

static bool delete_promise(zjs_promise_t *p)
{
    if (p) {
        DBG_PRINT("deleted promise, p=%p, argv=%p, argc=%u\n",
                   p, p->argv, p->argc);
        for (int i = 0; i < p->argc; ++i) {
            jerry_release_value(p->argv[i]);
        }
        jerry_release_value(p->promise_obj);
        ZJS_LIST_REMOVE(zjs_promise_t, zjs_promises, p);
        zjs_free(p->argv);
        zjs_free(p);
        return true;
    }
    return false;
}

void zjs_resolve_promise(zjs_promise_t *p, jerry_value_t arg)
{
    DBG_PRINT("resolve promise %p\n", p);
    jerry_resolve_or_reject_promise(p->promise_obj, arg, true);
    delete_promise(p);
}

void zjs_reject_promise(zjs_promise_t *p, jerry_value_t error)
{
    DBG_PRINT("reject promise %p\n", p);
    jerry_resolve_or_reject_promise(p->promise_obj, error, false);
    delete_promise(p);
}

jerry_value_t zjs_promise_init()
{
    return ZJS_UNDEFINED;
}

static void free_promise(zjs_promise_t *p)
{
    for (int i = 0; i < p->argc; ++i) {
        jerry_release_value(p->argv[i]);
    }
    jerry_release_value(p->promise_obj);
    zjs_free(p->argv);
    zjs_free(p);
}

void zjs_promise_cleanup()
{
    ZJS_LIST_FREE(zjs_promise_t, zjs_promises, free_promise);
}
