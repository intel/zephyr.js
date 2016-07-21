// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <string.h>

// JerryScript includes
#include "jerry-api.h"

// ZJS includes
#include "zjs_util.h"
#include "zjs_callbacks.h"

struct zjs_timer_t {
    struct nano_timer timer;
    void *timer_data;
    uint32_t interval;
    bool repeat;
    int32_t callback_id;
    jerry_value_t* args;
    uint32_t args_cnt;
    struct zjs_timer_t *next;
};

static struct zjs_timer_t *zjs_timers = NULL;

jerry_value_t* pre_timer(void* h, uint32_t* args_cnt)
{
    struct zjs_timer_t* handle = (struct zjs_timer_t*)h;
    *args_cnt = handle->args_cnt;
    return handle->args;
}

/*
 * Allocate a new timer and add it to list
 *
 * interval     Time until expiration (in ticks)
 * callback     JS callback function
 * repeat       Timeout or interval timer
 * args         Array of arguments to pass to timer callback function
 * cnt          Number of arguments in args
 */
static struct zjs_timer_t* add_timer(uint32_t interval,
                                     jerry_value_t callback,
                                     bool repeat,
                                     const jerry_value_t args[],
                                     uint32_t cnt)
{
    int i;
    struct zjs_timer_t *tm;

    tm = task_malloc(sizeof(struct zjs_timer_t));
    if (!tm) {
        PRINT("add_timer: out of memory allocating timer struct\n");
        return NULL;
    }

    nano_timer_init(&tm->timer, &tm->timer_data);
    tm->interval = interval;
    tm->repeat = repeat;
    tm->next = zjs_timers;
    tm->callback_id = zjs_add_callback(callback, tm, pre_timer, NULL);
    tm->args_cnt = cnt;
    tm->args = task_malloc(sizeof(jerry_value_t) * cnt);
    for (i = 0; i < cnt; ++i) {
        tm->args[i] = jerry_acquire_value(args[i + 2]);
    }

    zjs_timers = tm;

    nano_timer_start(&tm->timer, interval);
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
    for (struct zjs_timer_t **ptm = &zjs_timers; *ptm; ptm = &(*ptm)->next) {
        struct zjs_timer_t *tm = *ptm;
        if (id == tm->callback_id) {
            int i;
            nano_task_timer_stop(&tm->timer);
            *ptm = tm->next;
            for (i = 0; i < tm->args_cnt; ++i) {
                jerry_release_value(tm->args[i]);
            }
            task_free(tm->args);
            task_free(tm);
            return true;
        }
    }
    return false;
}

static void timer_free(const uintptr_t native_p)
{
    struct zjs_timer_t* handle = (struct zjs_timer_t*)native_p;
    delete_timer(handle->callback_id);
}

static jerry_value_t add_timer_helper(const jerry_value_t function_obj_val,
                                      const jerry_value_t this_val,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_cnt,
                                      bool repeat)
{
    if (args_cnt < 2 || !jerry_value_is_function(args_p[0]) ||
            !jerry_value_is_number(args_p[1])) {
        PRINT ("native_set_interval_handler: invalid arguments\n");
        return zjs_error("native_set_interval_handler: invalid arguments");
    }

    uint32_t interval = (uint32_t)(jerry_get_number_value(args_p[1]) / 1000 *
            CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    jerry_value_t callback = args_p[0];
    jerry_value_t timer_obj = jerry_create_object();

    struct zjs_timer_t* handle = add_timer(interval, callback, repeat, args_p, args_cnt - 2);
    if (handle->callback_id == -1) {
        PRINT ("native_set_interval_handler: timer alloc failed\n");
        return zjs_error("native_set_interval_handler: timer alloc failed");
    }
    jerry_set_object_native_handle(timer_obj, (uintptr_t)handle, timer_free);

    return timer_obj;
}

// native setInterval handler
static jerry_value_t native_set_interval_handler(const jerry_value_t function_obj_val,
                                                 const jerry_value_t this_val,
                                                 const jerry_value_t args_p[],
                                                 const jerry_length_t args_cnt)
{
    return add_timer_helper(function_obj_val,
                            this_val,
                            args_p,
                            args_cnt,
                            true);
}

// native setInterval handler
static jerry_value_t native_set_timeout_handler(const jerry_value_t function_obj_val,
                                                const jerry_value_t this_val,
                                                const jerry_value_t args_p[],
                                                const jerry_length_t args_cnt)
{
    return add_timer_helper(function_obj_val,
                            this_val,
                            args_p,
                            args_cnt,
                            false);
}

// native clearInterval handler
static jerry_value_t native_clear_interval_handler(const jerry_value_t function_obj_val,
                                                   const jerry_value_t this_val,
                                                   const jerry_value_t args_p[],
                                                   const jerry_length_t args_cnt)
{
    jerry_value_t timer_obj = args_p[0];
    struct zjs_timer_t* handle;

    if (!jerry_value_is_object(args_p[0])) {
        PRINT ("native_clear_interval_handler: invalid arguments\n");
        return jerry_create_undefined();
    }

    jerry_get_object_native_handle(timer_obj, (uintptr_t*)&handle);

    if (!delete_timer(handle->callback_id)) {
        PRINT ("native_clear_interval_handler: timer not found\n");
        return zjs_error("native_clear_interval_handler: timer not found");
    }

    return jerry_create_undefined();
}

void zjs_timers_process_events()
{
    for (struct zjs_timer_t *tm = zjs_timers; tm; tm = tm->next) {
        if (nano_task_timer_test(&tm->timer, TICKS_NONE)) {
            // timer has expired, signal the callback
            zjs_signal_callback(tm->callback_id);

            // reschedule or remove timer
            if (tm->repeat) {
                nano_timer_start(&tm->timer, tm->interval);
            } else {
                delete_timer(tm->callback_id);
            }
        }
    }
}

void zjs_timers_init()
{
    jerry_value_t global_obj_val = jerry_get_global_object();

    // create the C handler for setInterval JS call
    zjs_obj_add_function(global_obj_val, native_set_interval_handler, "setInterval");
    // create the C handler for clearInterval JS call
    zjs_obj_add_function(global_obj_val, native_clear_interval_handler, "clearInterval");
    // create the C handler for setTimeout JS call
    zjs_obj_add_function(global_obj_val, native_set_timeout_handler, "setTimeout");
    // create the C handler for clearTimeout JS call (same as clearInterval)
    zjs_obj_add_function(global_obj_val, native_clear_interval_handler, "clearTimeout");
}
