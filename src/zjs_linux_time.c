// Copyright (c) 2016, Intel Corporation.

#include "zjs_linux_time.h"
#include <time.h>

#define ZEPHYR_TICKS_PER_SEC

void zjs_port_timer_init(struct zjs_port_timer* timer, void* data)
{
    timer->data = data;
}

void zjs_port_timer_start(struct zjs_port_timer* timer, uint32_t interval)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timer->sec = now.tv_sec;
    timer->milli = now.tv_nsec / 1000000;
    timer->interval = interval * 1000 / 100;
}

void zjs_port_timer_stop(struct zjs_port_timer* timer)
{
    timer->interval = 0;
}

uint8_t zjs_port_timer_test(struct zjs_port_timer* timer, uint32_t ticks)
{
    uint32_t elapsed;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    elapsed = (1000 * (now.tv_sec - timer->sec)) + ((now.tv_nsec / 1000000) - timer->milli);

    if (elapsed >= timer->interval) {
        return 1;
    }
    return 0;
}

