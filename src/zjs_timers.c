// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#include <string.h>

#include "jerry.h"
#include "jerry-api.h"

#include "zjs_util.h"

#define MAX_NUMBER_TIMERS 10

static struct nano_timer timer;


struct zjs_timer_t {
    uint32_t interval;
    uint32_t ticks_remain;
    bool expire;
    jerry_api_object_t *callback;
    struct zjs_timer_t *next;
};


static struct zjs_timer_t *zjs_timers;

static jerry_api_object_t *
add_timer(uint32_t interval,
          jerry_api_object_t* callback)
{
    // effects:  allocate a new timer item and add to the timers list
    struct zjs_timer_t *tm;
    tm = task_malloc(sizeof(struct zjs_timer_t));
    if (!tm)
    {
        PRINT("error: out of memory allocating timer struct\n");
        return NULL;
    }

    tm->callback = jerry_api_acquire_object(callback);
    tm->interval = interval;
    tm->expire = false;
    tm->ticks_remain = interval;

    if (!zjs_timers)
    {
        tm->next = tm;
        zjs_timers = tm;
    }
    else
    {
        tm->next = zjs_timers->next;
        zjs_timers->next = tm;
    }
    return tm->callback;
}

static bool
delete_timer(jerry_api_object_t *objPtr)
{
    struct zjs_timer_t *tm = zjs_timers;
    if (!tm)
        return false;
    do
    {
        if (objPtr == tm->callback)
        {
            jerry_api_release_object(tm->callback);
            if (tm == zjs_timers)
            {
                if (zjs_timers->next == zjs_timers)
                    zjs_timers = NULL;
                else
                    zjs_timers = zjs_timers->next;
            }
            else
                tm->next = tm->next->next;

            task_free(tm);
            return true;
        }
        tm = tm->next;
    } while (tm != zjs_timers);
    return false;
}

// native setInterval handler
static bool
native_setInterval_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[1].type != JERRY_API_DATA_TYPE_FLOAT32 ||
        args_p[0].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT ("native_setInterval_handler: invalid arguments\n");
        return false;
    }

    uint32_t interval = (uint32_t)(args_p[1].u.v_float32 / 1000 *
                                   CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    jerry_api_object_t *callback = args_p[0].u.v_object;

    jerry_api_object_t *tid = add_timer(interval, callback);
    if (!tid)
    {
        // TODO: should throw an exception
        PRINT ("Error: out of timers\n");
        return false;
    }

    zjs_init_api_value_object(ret_val_p, tid);
    return true;
}

// native setInterval handler
static bool
native_clearInterval_handler(const jerry_api_object_t * function_obj_p,
         const jerry_api_value_t * this_p,
         jerry_api_value_t * ret_val_p,
         const jerry_api_value_t args_p[],
         const jerry_api_length_t args_cnt)
{
    if (args_p[0].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT ("native_setInterval_handler: invalid arguments\n");
        return false;
    }

    jerry_api_object_t *t = args_p[0].u.v_object;

    if (!delete_timer(t))
    {
        // TODO: should throw an exception
        PRINT ("Error: timer not found\n");
        return false;
    }

    return true;
}

bool zjs_timers_process_events()
{
    if (!zjs_timers)
        return false;

    struct zjs_timer_t *tm = zjs_timers;

    uint32_t ticks = tm->ticks_remain;
    while (tm->next != zjs_timers)
    {
        tm = tm->next;
        if (ticks > tm->ticks_remain && tm->ticks_remain != 0)
        {
            ticks = tm->ticks_remain;
        }
    }

    tm = zjs_timers;
    do
    {
        tm->ticks_remain = tm->ticks_remain - ticks;
        if (tm->ticks_remain < 1)
        {
            tm->ticks_remain = tm->interval;
            tm->expire = true;
        }
        tm = tm->next;
    } while (tm != zjs_timers);

    // setup next timer to be expired
    nano_timer_start(&timer, ticks);
    nano_timer_test(&timer, TICKS_UNLIMITED);

    // call callbacks
    tm = zjs_timers;
    do
    {
        if (tm->expire && tm->callback)
        {
            jerry_api_value_t ret_val;
            jerry_api_call_function(tm->callback, NULL, &ret_val, NULL, 0);
            tm->expire = false;
        }
        tm = tm->next;
    } while (tm != zjs_timers);
    return true;
}

void zjs_timers_init()
{
    void *timer_data[1];

    // initialize system timer
    nano_timer_init(&timer, timer_data);

    jerry_api_object_t *global_obj = jerry_api_get_global();

    // create the C handler for setInterval JS call
    zjs_obj_add_function(global_obj, native_setInterval_handler, "setInterval");

    // create the C handler for clearInterval JS call
    zjs_obj_add_function(global_obj, native_clearInterval_handler, "clearInterval");
}
