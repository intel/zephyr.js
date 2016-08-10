#ifndef ZJS_LINUX_TIME_H_
#define ZJS_LINUX_TIME_H_

#include "zjs_util.h"

struct zjs_port_timer {
    uint32_t sec;
    uint32_t milli;
    uint32_t interval;
    void* data;
};

void zjs_port_timer_init(struct zjs_port_timer* timer, void* data);

void zjs_port_timer_start(struct zjs_port_timer* timer, uint32_t interval);

void zjs_port_timer_stop(struct zjs_port_timer* timer);

uint8_t zjs_port_timer_test(struct zjs_port_timer* timer, uint32_t ticks);

#define ZJS_TICKS_NONE          0
#define CONFIG_SYS_CLOCK_TICKS_PER_SEC 100
#define zjs_sleep usleep

#endif /* ZJS_LINUX_TIME_H_ */
