// Copyright (c) 2016, Intel Corporation.

#include "zjs_linux_port.h"
#include <time.h>

#define ZEPHYR_TICKS_PER_SEC

void zjs_port_timer_start(zjs_port_timer_t* timer, uint32_t interval)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timer->sec = now.tv_sec;
    timer->milli = now.tv_nsec / 1000000;
    timer->interval = interval;
}

void zjs_port_timer_stop(zjs_port_timer_t* timer)
{
    timer->interval = 0;
}

uint8_t zjs_port_timer_test(zjs_port_timer_t* timer)
{
    uint32_t elapsed;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    elapsed = (1000 * (now.tv_sec - timer->sec)) + ((now.tv_nsec / 1000000) - timer->milli);

    if (elapsed >= timer->interval) {
        return elapsed;
    }
    return 0;
}
