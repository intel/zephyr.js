#ifndef ZJS_ZEPHYR_QUEUE_H_
#define ZJS_ZEPHYR_QUEUE_H_

#include <zephyr.h>

#define zjs_port_queue nano_fifo
#define zjs_port_init_queue nano_fifo_init
#define zjs_port_queue_put nano_fifo_put
#define zjs_port_queue_get nano_task_fifo_get

#endif
