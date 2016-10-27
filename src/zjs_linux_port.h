// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_LINUX_PORT_H_
#define ZJS_LINUX_PORT_H_

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

#define SIZE32_OF(x) (sizeof((x))/sizeof(uint32_t))

#define EAGAIN      11
#define EMSGSIZE    12
#define ENOSPC      28

struct zjs_port_ring_buf {
    uint32_t head;   /**< Index in buf for the head element */
    uint32_t tail;   /**< Index in buf for the tail element */
    uint32_t size;   /**< Size of buf in 32-bit chunks */
    uint32_t *buf;   /**< Memory region for stored entries */
    uint32_t mask;   /**< Modulo mask if size is a power of 2 */
};

void zjs_port_ring_buf_init(struct zjs_port_ring_buf* buf,
                            uint32_t size,
                            uint32_t* data);

int zjs_port_ring_buf_get(struct zjs_port_ring_buf* buf,
                          uint16_t* type,
                          uint8_t* value,
                          uint32_t* data,
                          uint8_t* size32);

int zjs_port_ring_buf_put(struct zjs_port_ring_buf* buf,
                          uint16_t type,
                          uint8_t value,
                          uint32_t* data,
                          uint8_t size32);

#endif /* ZJS_LINUX_PORT_H_ */

