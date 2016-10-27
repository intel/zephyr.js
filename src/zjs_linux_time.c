// Copyright (c) 2016, Intel Corporation.

#include "zjs_linux_port.h"
#include <time.h>

#define ZEPHYR_TICKS_PER_SEC

void zjs_port_timer_init(zjs_port_timer_t* timer, void* data)
{
    timer->data = data;
}

//clock_gettime is not implemented on OSX
#ifdef __MACH__
#include <sys/time.h>
#define CLOCK_MONOTONIC 1
int clock_gettime(int clk_id, struct timespec* t) {
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

void zjs_port_timer_start(zjs_port_timer_t* timer, uint32_t interval)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timer->sec = now.tv_sec;
    timer->milli = now.tv_nsec / 1000000;
    timer->interval = interval * 1000 / 100;
}

void zjs_port_timer_stop(zjs_port_timer_t* timer)
{
    timer->interval = 0;
}

uint8_t zjs_port_timer_test(zjs_port_timer_t* timer, uint32_t ticks)
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
