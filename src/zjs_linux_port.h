// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_LINUX_PORT_H_
#define ZJS_LINUX_PORT_H_

#include "zjs_util.h"
#include <unistd.h>
#include <time.h>

// define Zephyr numeric types we use
typedef uint32_t u32_t;

typedef struct zjs_port_timer {
    u32_t sec;
    u32_t milli;
    u32_t interval;
    void *data;
} zjs_port_timer_t;

#define zjs_port_timer_init(t) do {} while (0)

void zjs_port_timer_start(zjs_port_timer_t *timer, u32_t interval);

void zjs_port_timer_stop(zjs_port_timer_t *timer);

u8_t zjs_port_timer_test(zjs_port_timer_t *timer);

u32_t zjs_port_timer_get_uptime(void);

#define ZJS_TICKS_NONE          0
#define ZJS_TICKS_FOREVER       0
#define CONFIG_SYS_CLOCK_TICKS_PER_SEC 100
#define zjs_sleep usleep

#define SIZE32_OF(x) (sizeof((x))/sizeof(u32_t))

#define EAGAIN      11
#define EMSGSIZE    12
#define ENOSPC      28

struct zjs_port_ring_buf {
    u32_t head;   /**< Index in buf for the head element */
    u32_t tail;   /**< Index in buf for the tail element */
    u32_t size;   /**< Size of buf in 32-bit chunks */
    u32_t *buf;   /**< Memory region for stored entries */
    u32_t mask;   /**< Modulo mask if size is a power of 2 */
};

void zjs_port_ring_buf_init(struct zjs_port_ring_buf *buf,
                            u32_t size,
                            u32_t *data);

int zjs_port_ring_buf_get(struct zjs_port_ring_buf *buf,
                          u16_t *type,
                          u8_t *value,
                          u32_t *data,
                          u8_t *size32);

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
int zjs_port_ring_buf_put(struct zjs_port_ring_buf *buf,
                          u16_t type,
                          u8_t value,
                          u32_t *data,
                          u8_t size32);

#endif /* ZJS_LINUX_PORT_H_ */
