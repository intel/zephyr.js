// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_TIME_H_
#define ZJS_LINUX_TIME_H_

#include "zjs_util.h"
#include <unistd.h>

typedef struct zjs_port_timer {
    uint32_t sec;
    uint32_t milli;
    uint32_t interval;
    void* data;
} zjs_port_timer_t;

void zjs_port_timer_init(zjs_port_timer_t* timer, void* data);

void zjs_port_timer_start(zjs_port_timer_t* timer, uint32_t interval);

void zjs_port_timer_stop(zjs_port_timer_t* timer);

uint8_t zjs_port_timer_test(zjs_port_timer_t* timer, uint32_t ticks);

#define ZJS_TICKS_NONE          0
#define CONFIG_SYS_CLOCK_TICKS_PER_SEC 100
#define zjs_sleep usleep

#endif /* ZJS_LINUX_TIME_H_ */
