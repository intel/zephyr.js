// Copyright (c) 2016-2018, Intel Corporation.

// C includes
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include "zjs_zephyr_port.h"
#include <zephyr.h>
#else
#include "zjs_linux_port.h"
#endif

// JerryScript includes
#include "jerryscript.h"

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_util.h"

typedef struct zjs_timer {
    zjs_port_timer_t timer;
    jerry_value_t *argv;
    u32_t argc;
    zjs_callback_id callback_id;
    bool repeat;
#ifdef ZJS_LINUX_BUILD
    // FIXME - reverted patch #1542 to old timer implementation
    u32_t interval;
    bool completed;
#endif
    struct zjs_timer *next;
} zjs_timer_t;

static zjs_timer_t *zjs_timers = NULL;

static const jerry_object_native_info_t timer_type_info = {
    .free_cb = free_handle_nop
};

#ifdef ZJS_LINUX_BUILD
// FIXME - reverted patch #1542 to old timer implementation
jerry_value_t *pre_timer(void *h, u32_t *argc)
{
    zjs_timer_t *handle = (zjs_timer_t *)h;
    *argc = handle->argc;
    return handle->argv;
}

#define post_timer 0
#else
static bool delete_timer(zjs_timer_t *tm);

static void post_timer(void *handle, jerry_value_t ret_val)
{
    zjs_timer_t *timer = (zjs_timer_t *)handle;

    if (!timer->repeat) {
        delete_timer(timer);
    }
}

static void timer_callback(zjs_port_timer_t *handle)
{
    zjs_timer_t *timer = (zjs_timer_t *)handle->user_data;

    zjs_signal_callback(timer->callback_id, timer->argv,
                        sizeof(jerry_value_t) * timer->argc);

    if (!timer->repeat) {
        zjs_port_timer_stop(handle);
    }
}
#endif

/*
 * Allocate a new timer and add it to list
 *
 * interval     Time until expiration (in ticks)
 * callback     JS callback function
 * repeat       Timeout or interval timer
 * argv         Array of arguments to pass to timer callback function
 * argc         Number of arguments in argv
 */
static zjs_timer_t *add_timer(u32_t interval,
                              jerry_value_t callback,
                              jerry_value_t this,
                              bool repeat,
                              u32_t argc,
                              const jerry_value_t argv[])
{
    zjs_timer_t *tm = zjs_malloc(sizeof(zjs_timer_t));
    if (!tm) {
        ERR_PRINT("out of memory allocating timer struct\n");
        return NULL;
    }

#ifdef ZJS_LINUX_BUILD
    // FIXME - reverted patch #1542 to old timer implementation
    zjs_port_timer_init(&tm->timer, NULL);
    tm->interval = interval;
    tm->completed = false;
#else
    zjs_port_timer_init(&tm->timer, timer_callback);
    tm->timer.user_data = tm;
#endif
    tm->repeat = repeat;
    tm->next = NULL;
    tm->argc = argc;
    if (tm->argc) {
        tm->argv = zjs_malloc(sizeof(jerry_value_t) * argc);
        if (!tm->argv) {
            zjs_free(tm);
            ERR_PRINT("out of memory allocating timer args\n");
            return NULL;
        }
        for (int i = 0; i < argc; ++i) {
            tm->argv[i] = jerry_acquire_value(argv[i + 2]);
        }
    } else {
        tm->argv = NULL;
    }

    if (tm->repeat) {
        tm->callback_id = zjs_add_callback(callback, this, tm, NULL);
    } else {
        tm->callback_id = zjs_add_callback_once(callback, this, tm, post_timer);
    }

    ZJS_LIST_APPEND(zjs_timer_t, zjs_timers, tm);

    DBG_PRINT("add timer, id=%d, interval=%u, repeat=%u, argv=%p, argc=%u\n",
              tm->callback_id, interval, repeat, argv, argc);
#ifdef ZJS_LINUX_BUILD
    // FIXME - reverted patch #1542 to old timer implementation
    zjs_port_timer_start(&tm->timer, interval, 0);
#else
    zjs_port_timer_start(&tm->timer, repeat ? interval : 0, interval);
#endif
    return tm;
}

/*
 * Remove a timer from the list
 *
 * id           ID of timer, returned from add_timer
 *
 * returns      True if the timer was removed successfully (if the ID is valid)
 */
static bool delete_timer(zjs_timer_t *tm)
{
    if (tm) {
        // If the timer isn't in the list, its already been deleted
        if (zjs_timers == NULL ||
            !ZJS_LIST_REMOVE(zjs_timer_t, zjs_timers, tm)) {
            return false;
        }
        zjs_port_timer_stop(&tm->timer);
        for (int i = 0; i < tm->argc; ++i) {
            jerry_release_value(tm->argv[i]);
        }
        // remove callbacks except for expired once timers
#ifdef ZJS_LINUX_BUILD
        // FIXME - reverted patch #1542 to old timer implementation
        if (tm->repeat || !tm->completed) {
#else
        if (tm->repeat) {
#endif
            zjs_remove_callback(tm->callback_id);
        }
        zjs_free(tm->argv);
        zjs_free(tm);
        return true;
    }
    return false;
}

static ZJS_DECL_FUNC_ARGS(add_timer_helper, bool repeat)
{
    // args: callback, time in milliseconds[, pass-through args]
    ZJS_VALIDATE_ARGS(Z_FUNCTION, Z_NUMBER);

    u32_t interval = (u32_t)(jerry_get_number_value(argv[1]));
    jerry_value_t callback = argv[0];
    jerry_value_t timer_obj = zjs_create_object();

#ifdef ZJS_FIND_FUNC_NAME
    if (repeat) {
        zjs_obj_add_string(callback, ZJS_HIDDEN_PROP("function_name"),
                           "setInterval");
    } else {
        zjs_obj_add_string(callback, ZJS_HIDDEN_PROP("function_name"),
                           "setTimeout");
    }
#endif
    zjs_timer_t *handle = add_timer(interval, callback, this, repeat,
                                    argc - 2, argv);
    if (handle->callback_id == -1)
        return zjs_error("timer alloc failed");
    jerry_set_object_native_pointer(timer_obj, handle, &timer_type_info);

    return timer_obj;
}

// native setInterval handler
static ZJS_DECL_FUNC(native_set_interval_handler)
{
    return ZJS_CHAIN_FUNC_ARGS(add_timer_helper, true);
}

// native setInterval handler
static ZJS_DECL_FUNC(native_set_timeout_handler)
{
    return ZJS_CHAIN_FUNC_ARGS(add_timer_helper, false);
}

// native clearInterval handler
static ZJS_DECL_FUNC(native_clear_interval_handler)
{
    // args: timer object
    // FIXME: timers should be ints, not objects!
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    ZJS_GET_HANDLE_OR_NULL(argv[0], zjs_timer_t, handle, timer_type_info);

    if (!delete_timer(handle))
        DBG_PRINT("timer not found\n");

    return ZJS_UNDEFINED;
}

#ifdef ZJS_LINUX_BUILD
// FIXME - reverted patch #1542 to old timer implementation
s32_t zjs_timers_process_events()
{
    s32_t wait = ZJS_TICKS_FOREVER;
    for (zjs_timer_t *tm = zjs_timers; tm; tm = tm->next) {
        if (tm->completed) {
            delete_timer(tm);
        } else if (zjs_port_timer_test(&tm->timer) > 0) {
            // timer has expired, signal the callback
            DBG_PRINT("signaling timer. id=%d, argv=%p, argc=%u\n",
                      tm->callback_id, tm->argv, tm->argc);
            zjs_signal_callback(tm->callback_id, tm->argv,
                                tm->argc * sizeof(jerry_value_t));

            // reschedule or remove timer
            if (tm->repeat) {
                zjs_port_timer_start(&tm->timer, tm->interval, 0);
            } else {
                // delete this timer next time around
                tm->completed = true;
            }
        }

        /*
         * On Linux, we don't block on a semaphore; we just need to return if
         * there were any timers serviced. If there was any timers in the list
         * then we signal.
         */
        wait = 1;
    }

    return !(wait == ZJS_TICKS_FOREVER);
}
#endif

void zjs_timers_init()
{
    ZVAL global_obj = jerry_get_global_object();

    // create the C handler for setInterval JS call
    zjs_obj_add_function(global_obj, "setInterval",
                         native_set_interval_handler);
    // create the C handler for clearInterval JS call
    zjs_obj_add_function(global_obj, "clearInterval",
                         native_clear_interval_handler);
    // create the C handler for setTimeout JS call
    zjs_obj_add_function(global_obj, "setTimeout", native_set_timeout_handler);
    // create the C handler for clearTimeout JS call (same as clearInterval)
    zjs_obj_add_function(global_obj, "clearTimeout",
                         native_clear_interval_handler);
}

static void free_timer(zjs_timer_t *tm)
{
    for (int i = 0; i < tm->argc; ++i) {
        jerry_release_value(tm->argv[i]);
    }
    zjs_port_timer_stop(&tm->timer);
    zjs_remove_callback(tm->callback_id);
    zjs_free(tm->argv);
    zjs_free(tm);
}

void zjs_timers_cleanup()
{
    ZJS_LIST_FREE(zjs_timer_t, zjs_timers, free_timer);
}
