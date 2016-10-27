// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_ZEPHYR_PORT_H_
#define ZJS_ZEPHYR_PORT_H_

#include <zephyr.h>

#define zjs_port_timer_t        struct nano_timer
#define zjs_port_timer_init     nano_timer_init
#define zjs_port_timer_start    nano_timer_start
#define zjs_port_timer_stop     nano_task_timer_stop
#define zjs_port_timer_test     nano_task_timer_test
#define ZJS_TICKS_NONE          TICKS_NONE
#define zjs_sleep               task_sleep

#define zjs_port_ring_buf ring_buf
#define zjs_port_ring_buf_init sys_ring_buf_init
#define zjs_port_ring_buf_get sys_ring_buf_get
#define zjs_port_ring_buf_put sys_ring_buf_put

#endif /* ZJS_ZEPHYR_PORT_H_ */

