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


typedef struct {
    uint32_t interval;
    uint32_t ticks_remain;
    bool expire;
    jerry_api_object_t *callback;
} zjs_timer_t;


static zjs_timer_t zjs_timers[MAX_NUMBER_TIMERS];

static jerry_api_object_t *
add_timer(uint32_t interval,
          jerry_api_object_t* callback)
{
    for (int i = 0; i < MAX_NUMBER_TIMERS; i++)
    {
        zjs_timer_t *t = &zjs_timers[i];
        if (!t->callback)
        {
            // Found an empty slot.
            t->callback = jerry_api_acquire_object(callback);
            t->interval = interval;
            t->expire = false;
            t->ticks_remain = interval;
            return t->callback;
        }
    }
    return NULL;
}

static bool
delete_timer(jerry_api_object_t *objPtr)
{
    PRINT ("Delete timer is called\n");
    for (int i = 0; i < MAX_NUMBER_TIMERS; i++)
    {
        zjs_timer_t *t = &zjs_timers[i];
        if (objPtr == t->callback)
        {
            PRINT ("timer found\n");
            jerry_api_release_object(t->callback);
            t->callback = NULL;
            t->interval = 0;
            t->expire = false;
            t->ticks_remain = 0;
            return true;
        }
    }
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

    PRINT ("clear interval is called\n");
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
    int i;

    /* find the least amount of ticks remain in the list */
    uint32_t ticks = 0xffffffff;
    zjs_timer_t *t = NULL;
    for (i = 0; i < MAX_NUMBER_TIMERS; i++)
    {
        t = &zjs_timers[i];
        if (ticks > t->ticks_remain && t->ticks_remain != 0)
        {
            ticks = t->ticks_remain;
        }
    }

    if (ticks == 0xffffffff)
        return false;

    for (i = 0; i < MAX_NUMBER_TIMERS; i++)
    {
        t = &zjs_timers[i];
        t->ticks_remain = t->ticks_remain - ticks;
        if (t->ticks_remain < 1 && t->callback)
        {
            t->ticks_remain = t->interval;
            t->expire = true;
        }
    }

    // setup next timer to be expired
    nano_timer_start(&timer, ticks);
    nano_timer_test(&timer, TICKS_UNLIMITED);

    // call callbacks
    for (i = 0; i < MAX_NUMBER_TIMERS; i++)
    {
        t = &zjs_timers[i];
        if (t->expire && t->callback)
        {
            jerry_api_value_t ret_val;
            jerry_api_call_function(t->callback, NULL, &ret_val, NULL, 0);
            t->expire = false;
        }
    }

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
