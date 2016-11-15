// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_ZEPHYR_PORT_H_
#define ZJS_ZEPHYR_PORT_H_

#include <zephyr.h>

#define zjs_port_timer_t                struct k_timer
#define zjs_port_timer_init(t)          k_timer_init(t, NULL, NULL)
#define zjs_port_timer_start(t, i)      k_timer_start(t, i, i)
#define zjs_port_timer_stop             k_timer_stop
#define zjs_port_timer_test             k_timer_status_get
#define ZJS_TICKS_NONE                  TICKS_NONE
#define zjs_sleep                       k_sleep

#define zjs_port_ring_buf ring_buf
#define zjs_port_ring_buf_init sys_ring_buf_init
#define zjs_port_ring_buf_get sys_ring_buf_get
#define zjs_port_ring_buf_put sys_ring_buf_put

#endif /* ZJS_ZEPHYR_PORT_H_ */
