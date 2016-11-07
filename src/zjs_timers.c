// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif

#include <string.h>

// JerryScript includes
#include "jerry-api.h"

// ZJS includes
#include "zjs_util.h"
#include "zjs_callbacks.h"

typedef struct zjs_timer {
    zjs_port_timer_t timer;
    void *timer_data;
    jerry_value_t* argv;
    uint32_t argc;
    uint32_t interval;
    int32_t callback_id;
    bool repeat;
    bool completed;
    struct zjs_timer *next;
} zjs_timer_t;

static zjs_timer_t *zjs_timers = NULL;

jerry_value_t* pre_timer(void* h, uint32_t* argc)
{
    zjs_timer_t* handle = (zjs_timer_t*)h;
    *argc = handle->argc;
    return handle->argv;
}

/*
 * Allocate a new timer and add it to list
 *
 * interval     Time until expiration (in ticks)
 * callback     JS callback function
 * repeat       Timeout or interval timer
 * argv         Array of arguments to pass to timer callback function
 * argc         Number of arguments in argv
 */
static zjs_timer_t* add_timer(uint32_t interval,
                              jerry_value_t callback,
                              jerry_value_t this,
                              bool repeat,
                              const jerry_value_t argv[],
                              uint32_t argc)
{
    int i;
    zjs_timer_t *tm;

    tm = zjs_malloc(sizeof(zjs_timer_t));
    if (!tm) {
        PRINT("add_timer: out of memory allocating timer struct\n");
        return NULL;
    }

    zjs_port_timer_init(&tm->timer, &tm->timer_data);
    tm->interval = interval;
    tm->repeat = repeat;
    tm->completed = false;
    tm->next = zjs_timers;
    tm->callback_id = zjs_add_callback(callback, this, tm, NULL);
    tm->argc = argc;
    if (tm->argc) {
        tm->argv = zjs_malloc(sizeof(jerry_value_t) * argc);
        for (i = 0; i < argc; ++i) {
            tm->argv[i] = jerry_acquire_value(argv[i + 2]);
        }
    } else {
        tm->argv = NULL;
    }

    zjs_timers = tm;

    DBG_PRINT("adding timer. id=%li, interval=%lu, repeat=%u, argv=%p, argc=%lu\n",
            tm->callback_id, interval, repeat, argv, argc);
    zjs_port_timer_start(&tm->timer, interval);
    return tm;
}

/*
 * Remove a timer from the list
 *
 * id           ID of timer, returned from add_timer
 *
 * returns      True if the timer was removed successfully (if the ID is valid)
 */
static bool delete_timer(int32_t id)
{
    for (zjs_timer_t **ptm = &zjs_timers; *ptm; ptm = &(*ptm)->next) {
        zjs_timer_t *tm = *ptm;
        if (id == tm->callback_id) {
            DBG_PRINT("removing timer. id=%li\n", tm->callback_id);
            zjs_port_timer_stop(&tm->timer);
            *ptm = tm->next;
            for (int i = 0; i < tm->argc; ++i) {
                jerry_release_value(tm->argv[i]);
            }
            zjs_remove_callback(tm->callback_id);
            zjs_free(tm->argv);
            zjs_free(tm);
            return true;
        }
    }
    return false;
}

void zjs_timers_cleanup()
{
    while (zjs_timers) {
        zjs_timer_t *tm = zjs_timers;
        for (int i = 0; i < tm->argc; ++i) {
            jerry_release_value(tm->argv[i]);
        }
        zjs_port_timer_stop(&tm->timer);
        zjs_remove_callback(tm->callback_id);
        zjs_free(tm->argv);
        zjs_timers = tm->next;
        zjs_free(tm);
    }
}

static jerry_value_t add_timer_helper(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc,
                                      bool repeat)
{
    if (argc < 2 || !jerry_value_is_function(argv[0]) ||
            !jerry_value_is_number(argv[1]))
        return zjs_error("native_set_interval_handler: invalid arguments");

    uint32_t interval = (uint32_t)(jerry_get_number_value(argv[1]) / 1000 *
            CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    jerry_value_t callback = argv[0];
    jerry_value_t timer_obj = jerry_create_object();

    zjs_timer_t* handle = add_timer(interval, callback, this, repeat, argv, argc - 2);
    if (handle->callback_id == -1)
        return zjs_error("native_set_interval_handler: timer alloc failed");
    jerry_set_object_native_handle(timer_obj, (uintptr_t)handle, NULL);

    return timer_obj;
}

// native setInterval handler
static jerry_value_t native_set_interval_handler(const jerry_value_t function_obj,
                                                 const jerry_value_t this,
                                                 const jerry_value_t argv[],
                                                 const jerry_length_t argc)
{
    return add_timer_helper(function_obj,
                            this,
                            argv,
                            argc,
                            true);
}

// native setInterval handler
static jerry_value_t native_set_timeout_handler(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    return add_timer_helper(function_obj,
                            this,
                            argv,
                            argc,
                            false);
}

// native clearInterval handler
static jerry_value_t native_clear_interval_handler(const jerry_value_t function_obj,
                                                   const jerry_value_t this,
                                                   const jerry_value_t argv[],
                                                   const jerry_length_t argc)
{
    jerry_value_t timer_obj = argv[0];
    zjs_timer_t* handle;

    if (!jerry_value_is_object(argv[0])) {
        PRINT ("native_clear_interval_handler: invalid arguments\n");
        return jerry_create_undefined();
    }

    if (!jerry_get_object_native_handle(timer_obj, (uintptr_t*)&handle)) {
        return zjs_error("native_clear_interval_handler(): native handle not found");
    }

    if (!delete_timer(handle->callback_id))
        return zjs_error("native_clear_interval_handler: timer not found");

    return jerry_create_undefined();
}

void zjs_timers_process_events()
{
#ifdef DEBUG_BUILD
#ifndef ZJS_LINUX_BUILD
    extern void update_print_timer(void);
    update_print_timer();
#endif
#endif
    for (zjs_timer_t *tm = zjs_timers; tm; tm = tm->next) {
        if (tm->completed) {
            delete_timer(tm->callback_id);
        }
        else if (zjs_port_timer_test(&tm->timer, ZJS_TICKS_NONE)) {
            // timer has expired, signal the callback
            DBG_PRINT("signaling timer. id=%li, argv=%p, argc=%lu\n",
                    tm->callback_id, tm->argv, tm->argc);
            zjs_signal_callback(tm->callback_id, tm->argv, tm->argc * sizeof(jerry_value_t));

            // reschedule or remove timer
            if (tm->repeat) {
                zjs_port_timer_start(&tm->timer, tm->interval);
            } else {
                // delete this timer next time around
                tm->completed = true;
            }
        }
    }
}

void zjs_timers_init()
{
    jerry_value_t global_obj = jerry_get_global_object();

    // create the C handler for setInterval JS call
    zjs_obj_add_function(global_obj, native_set_interval_handler,
                         "setInterval");
    // create the C handler for clearInterval JS call
    zjs_obj_add_function(global_obj, native_clear_interval_handler,
                         "clearInterval");
    // create the C handler for setTimeout JS call
    zjs_obj_add_function(global_obj, native_set_timeout_handler, "setTimeout");
    // create the C handler for clearTimeout JS call (same as clearInterval)
    zjs_obj_add_function(global_obj, native_clear_interval_handler,
                         "clearTimeout");
    jerry_release_value(global_obj);
}
