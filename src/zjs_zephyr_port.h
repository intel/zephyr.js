// Copyright (c) 2016-2017, Intel Corporation.

#ifndef ZJS_ZEPHYR_PORT_H_
#define ZJS_ZEPHYR_PORT_H_

#include <zephyr.h>

#define zjs_port_timer_cb               k_timer_expiry_t
#define zjs_port_timer_t                struct k_timer
#define zjs_port_timer_init(t, f)       k_timer_init(t, f, NULL)
#define zjs_port_timer_start(t, i, r)   k_timer_start(t, r, r)
#define zjs_port_timer_stop             k_timer_stop
#define zjs_port_timer_get_uptime       k_uptime_get_32
#define ZJS_TICKS_NONE                  TICKS_NONE
#define ZJS_TICKS_FOREVER               K_FOREVER
#define zjs_sleep                       k_sleep

#define zjs_port_ring_buf               ring_buf
#define zjs_port_ring_buf_init          sys_ring_buf_init
#define zjs_port_ring_buf_get           sys_ring_buf_get
#define zjs_port_ring_buf_put           sys_ring_buf_put

#define zjs_port_sem_init               k_sem_init
#define zjs_port_sem_take               k_sem_take
#define zjs_port_sem_give               k_sem_give
#define zjs_port_sem                    struct k_sem

#define zjs_port_get_thread_id          k_current_get

#endif /* ZJS_ZEPHYR_PORT_H_ */
