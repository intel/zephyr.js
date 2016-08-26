#include "zjs_linux_queue.h"

void zjs_port_init_queue(struct zjs_port_queue* queue)
{
    queue->head = NULL;
    queue->tail = NULL;
}

void zjs_port_queue_put(struct zjs_port_queue* queue, void* data)
{
    struct linux_queue_element* new = zjs_malloc(sizeof(struct linux_queue_element));
    if (!new) {
        PRINT("[queue] zjs_port_queue_put(): Could not allocate new queue element\n");
        return;
    }
    new->data = data;
    new->next = NULL;
    if (queue->head == NULL && queue->tail == NULL) {
        queue->head = new;
        queue->tail = new;
    } else {
        queue->tail->next = new;
        queue->tail = new;
    }
}

void* zjs_port_queue_get(struct zjs_port_queue* queue, uint32_t wait)
{
    if (queue->head == NULL && queue->tail == NULL) {
        // Empty list
        return NULL;
    } else {
        void* data;
        struct linux_queue_element* head = queue->head;
        struct linux_queue_element* next = head->next;
        data = head->data;
        zjs_free(head);
        queue->head = next;
        if (queue->head == NULL) {
            queue->head = NULL;
            queue->tail = NULL;
        }
        return data;
    }
}
