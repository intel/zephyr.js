#ifndef SRC_ZJS_LINUX_QUEUE_H_
#define SRC_ZJS_LINUX_QUEUE_H_

#include "zjs_util.h"

struct linux_queue_element {
    void* data;
    struct linux_queue_element* next;
};

struct zjs_port_queue {
    struct linux_queue_element* head;
    struct linux_queue_element* tail;
};

void zjs_port_init_queue(struct zjs_port_queue* queue);

void zjs_port_queue_put(struct zjs_port_queue* queue, void* data);

void* zjs_port_queue_get(struct zjs_port_queue* queue, uint32_t wait);

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

#endif /* SRC_ZJS_LINUX_QUEUE_H_ */
