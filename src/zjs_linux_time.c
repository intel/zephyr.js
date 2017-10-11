// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <time.h>
#include <signal.h>
#include <stdio.h>

// ZJS includes
#include "zjs_linux_port.h"

#define ZEPHYR_TICKS_PER_SEC

static void timer_signal(int signal, siginfo_t *info, void *oldaction)
{
    zjs_port_timer_t *timer = info->si_value.sival_ptr;
    timer->callback(timer);
}

void zjs_port_timer_init(zjs_port_timer_t *timer, zjs_port_timer_cb cb)
{
    timer->sa.sa_sigaction = timer_signal;
    timer->sa.sa_flags = SA_SIGINFO;
    sigemptyset(&timer->sa.sa_mask);
    sigaction(SIGRTMIN, &timer->sa, NULL);

    timer->callback = cb;
    timer->se.sigev_signo = SIGRTMIN;
    timer->se.sigev_notify = SIGEV_SIGNAL;
    timer->se.sigev_value.sival_ptr = timer;
    timer_create(CLOCK_REALTIME, &timer->se, &timer->time);
}

void zjs_port_timer_start(zjs_port_timer_t *timer, u32_t interval, u32_t repeat)
{
    /*
     * Signals require > 0 timeout, so if a zero timeout is passed we have to
     * just set it to 1.
     */
    if (repeat == 0)
        repeat++;

    uint32_t rsec = repeat / 1000;
    uint32_t rms = repeat - (rsec * 1000);

    timer->ti.it_interval.tv_sec = rsec;
    timer->ti.it_interval.tv_nsec = rms * 1000000;
    timer->ti.it_value.tv_sec = rsec;
    timer->ti.it_value.tv_nsec = rms * 1000000;

    timer_settime(timer->time, 0, &timer->ti, NULL);
}

void zjs_port_timer_stop(zjs_port_timer_t *timer)
{
    timer->ti.it_interval.tv_sec = 0;
    timer->ti.it_interval.tv_nsec = 0;
    timer->ti.it_value.tv_sec = 0;
    timer->ti.it_value.tv_nsec = 0;
    timer_settime(timer->time, 0, &timer->ti, NULL);
}

u32_t zjs_port_timer_get_uptime(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (1000 * now.tv_sec) + (now.tv_nsec / 1000000);
}
