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
