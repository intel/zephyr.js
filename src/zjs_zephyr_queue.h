#ifndef ZJS_ZEPHYR_QUEUE_H_
#define ZJS_ZEPHYR_QUEUE_H_

#include <zephyr.h>

#define zjs_port_queue nano_fifo
#define zjs_port_init_queue nano_fifo_init
#define zjs_port_queue_put nano_fifo_put
#define zjs_port_queue_get nano_task_fifo_get

#define zjs_port_ring_buf ring_buf
#define zjs_port_ring_buf_init sys_ring_buf_init
#define zjs_port_ring_buf_get sys_ring_buf_get
#define zjs_port_ring_buf_put sys_ring_buf_put

#endif
