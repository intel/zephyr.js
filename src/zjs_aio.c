// Copyright (c) 2016, Intel Corporation.
#ifndef QEMU_BUILD
// Zephyr includes
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_aio.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

DEFINE_SEMAPHORE(SEM_AIO_BLOCK);

static uint32_t pin_values[ARC_AIO_LEN] = {};

struct zjs_cb_list_item {
    char event_type[20];
    jerry_object_t *pin_obj;
    struct zjs_callback zjs_cb;
    double value;
    struct zjs_cb_list_item *next;
};

static struct zjs_cb_list_item *zjs_cb_list = NULL;

static struct zjs_cb_list_item *zjs_aio_callback_alloc()
{
    // effects: allocates a new callback list item and adds it to the list
    struct zjs_cb_list_item *item;
    item = task_malloc(sizeof(struct zjs_cb_list_item));
    if (!item) {
        PRINT("error: out of memory allocating callback struct\n");
        return NULL;
    } else {
        memset(item, 0, sizeof(struct zjs_cb_list_item));
    }

    item->next = zjs_cb_list;
    zjs_cb_list = item;
    return item;
}

static void zjs_aio_callback_free(uintptr_t handle)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the callback list item for the given pin object
    struct zjs_cb_list_item **pItem = &zjs_cb_list;
    while (*pItem) {
        if ((uintptr_t)*pItem == handle) {
            *pItem = (*pItem)->next;
            task_free((void *)handle);
        }
        pItem = &(*pItem)->next;
    }
}

struct zjs_cb_list_item *zjs_aio_get_callback_item(uint32_t pin, const char *event_type)
{
    // calls the JS callback registered in the struct
    struct zjs_cb_list_item **pItem = &zjs_cb_list;
    while (*pItem) {
        jerry_object_t *obj = (*pItem)->pin_obj;
        uint32_t pin_val;
        zjs_obj_get_uint32(obj, "pin", &pin_val);

        if (pin == pin_val) {
            if (event_type == NULL ||
                !strcmp((*pItem)->event_type, event_type)) {
                return (*pItem);
            }
        }
        pItem = &(*pItem)->next;
    }

    return NULL;
}

static void zjs_aio_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context
    //  effects: handles execution of the JS callback when ready
    struct zjs_cb_list_item *mycb = CONTAINER_OF(cb, struct zjs_cb_list_item,
                                                 zjs_cb);
    jerry_value_t arg = jerry_create_number_value(mycb->value);
    jerry_value_t rval = jerry_call_function(mycb->zjs_cb.js_callback, NULL, &arg, 1);
    if (!jerry_value_is_error(rval))
        jerry_release_value(rval);
    jerry_release_object(mycb->zjs_cb.js_callback);
    zjs_aio_callback_free((uintptr_t)mycb);
}

static void zjs_aio_emit_event(struct zjs_callback *cb)
{
    struct zjs_cb_list_item *mycb = CONTAINER_OF(cb, struct zjs_cb_list_item,
                                                 zjs_cb);
    jerry_value_t arg = jerry_create_number_value(mycb->value);
    jerry_value_t rval = jerry_call_function(mycb->zjs_cb.js_callback, NULL, &arg, 1);
    if (!jerry_value_is_error(rval))
        jerry_release_value(rval);
}

int zjs_aio_ipm_send(uint32_t type, uint32_t pin, uint32_t value) {
    struct zjs_ipm_message msg;
    msg.block = false;
    msg.type = type;
    msg.pin = pin;
    msg.value = value;
    return zjs_ipm_send(MSG_ID_AIO, &msg, sizeof(msg));
}

int zjs_aio_ipm_send_blocking(uint32_t type, uint32_t pin, uint32_t value) {
    struct zjs_ipm_message msg;
    msg.block = true;
    msg.type = type;
    msg.pin = pin;
    msg.value = value;
    return zjs_ipm_send(MSG_ID_AIO, &msg, sizeof(msg));
}

// callback that gets updated of latest analog value from pin
void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_AIO)
        return;

    struct zjs_ipm_message *msg = (struct zjs_ipm_message *) data;

    if (msg->type == TYPE_AIO_OPEN_SUCCESS) {
        PRINT("pin %lu is opened\n", msg->pin);
    } else if (msg->type == TYPE_AIO_PIN_READ_SUCCESS) {
        if (msg->pin < ARC_AIO_MIN || msg->pin > ARC_AIO_MAX) {
            PRINT("X86 - pin #%lu out of range\n", msg->pin);
            return;
        }

        if (msg->block) {
            pin_values[msg->pin - ARC_AIO_MIN] = msg->value;
        }
        else {
            struct zjs_cb_list_item *mycb = zjs_aio_get_callback_item(msg->pin, NULL);
            if (mycb && mycb->zjs_cb.js_callback) {
                // TODO: ensure that there is no outstanding callback of this
                //   type or else we may be overwriting the value it should have
                //   reported
                mycb->value = (double)msg->value;
                zjs_queue_callback(&mycb->zjs_cb);
            }
        }
    } else if (msg->type == TYPE_AIO_PIN_SUBSCRIBE_SUCCESS) {
        PRINT("subscribed to events on pin %lu\n", msg->pin);
    } else if (msg->type == TYPE_AIO_PIN_UNSUBSCRIBE_SUCCESS) {
        PRINT("unsubscribed to events on pin %lu\n", msg->pin);
    } else if (msg->type == TYPE_AIO_PIN_EVENT_VALUE_CHANGE) {
        struct zjs_cb_list_item *mycb = zjs_aio_get_callback_item(msg->pin, "change");
        if (mycb && mycb->zjs_cb.js_callback) {
            mycb->value = (double)msg->value;
            zjs_queue_callback(&mycb->zjs_cb);
        } else {
            PRINT("onChange event callback not found\n");
        }
    } else if (msg->type == TYPE_AIO_OPEN_FAIL ||
               msg->type == TYPE_AIO_PIN_READ_FAIL ||
               msg->type == TYPE_AIO_PIN_SUBSCRIBE_FAIL) {
        PRINT("Error - failed to perform operation %lu\n", msg->type);
    } else {
        PRINT("IPM message not handled %lu\n", msg->type);
    }

    if (msg->block) {
        // un-block sync api
        isr_sem_give(SEM_AIO_BLOCK);
    }
}

jerry_object_t *zjs_aio_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_AIO, ipm_msg_receive_callback);

    // create global AIO object
    jerry_object_t *aio_obj = jerry_create_object();
    zjs_obj_add_function(aio_obj, zjs_aio_open, "open");
    return aio_obj;
}

bool zjs_aio_open(const jerry_object_t *function_obj_p,
                  const jerry_value_t this_val,
                  const jerry_value_t args_p[],
                  const jerry_length_t args_cnt,
                  jerry_value_t *ret_val_p)
{
    if (args_cnt < 1 || !jerry_value_is_object(args_p[0])) {
        PRINT("zjs_aio_open: invalid arguments\n");
        return false;
    }

    jerry_object_t *data = jerry_get_object_value(args_p[0]);

    uint32_t device;
    if (!zjs_obj_get_uint32(data, "device", &device)) {
        PRINT("zjs_aio_open: missing required field (device)\n");
        return false;
    }

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin)) {
        PRINT("zjs_aio_open: missing required field (pin)\n");
        return false;
    }

    const int BUFLEN = 32;
    char buffer[BUFLEN];

    if (zjs_obj_get_string(data, "name", buffer, BUFLEN)) {
        buffer[0] = '\0';
    }

    char *name = buffer;
    bool raw = false;
    zjs_obj_get_boolean(data, "raw", &raw);

    // send IPM message to the ARC side
    zjs_aio_ipm_send_blocking(TYPE_AIO_OPEN, pin, 0);
    // block until reply or timeout
    if (task_sem_take(SEM_AIO_BLOCK, 500) == RC_TIME) {
        PRINT("Reply from ARC timed out!");
        return false;
    }

    // create the AIOPin object
    jerry_object_t *pinobj = jerry_create_object();
    zjs_obj_add_function(pinobj, zjs_aio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_aio_pin_read_async, "read_async");
    zjs_obj_add_function(pinobj, zjs_aio_pin_abort, "abort");
    zjs_obj_add_function(pinobj, zjs_aio_pin_close, "close");
    zjs_obj_add_function(pinobj, zjs_aio_pin_on, "on");
    zjs_obj_add_string(pinobj, name, "name");
    zjs_obj_add_number(pinobj, device, "device");
    zjs_obj_add_number(pinobj, pin, "pin");
    zjs_obj_add_boolean(pinobj, raw, "raw");

    *ret_val_p = jerry_create_object_value(pinobj);
    return true;
}

bool zjs_aio_pin_read(const jerry_object_t *function_obj_p,
                      const jerry_value_t this_val,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt,
                      jerry_value_t *ret_val_p)
{
    jerry_object_t *obj = jerry_get_object_value(this_val);

    uint32_t device, pin;
    zjs_obj_get_uint32(obj, "device", &device);
    zjs_obj_get_uint32(obj, "pin", &pin);

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        PRINT("pin #%lu out of range\n", pin);
        return false;
    }

    // send IPM message to the ARC side
    zjs_aio_ipm_send_blocking(TYPE_AIO_PIN_READ, pin, 0);
    // block until reply or timeout
    if (task_sem_take(SEM_AIO_BLOCK, 500) == RC_TIME) {
        PRINT("Reply from ARC timed out!");
        return false;
    }

    double value;
    value = (double) pin_values[pin - ARC_AIO_MIN];

    *ret_val_p = jerry_create_number_value(value);
    return true;
}

bool zjs_aio_pin_abort(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p)
{
    // NO-OP
    return true;
}

bool zjs_aio_pin_close(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p)
{
    // NO-OP
    return true;
}

bool zjs_aio_pin_on(const jerry_object_t *function_obj_p,
                    const jerry_value_t this_val,
                    const jerry_value_t args_p[],
                    const jerry_length_t args_cnt,
                    jerry_value_t *ret_val_p)
{
    if (args_cnt < 2 ||
        !jerry_value_is_string(args_p[0]) ||
        (!jerry_value_is_object(args_p[1]) &&
         !jerry_value_is_null(args_p[1]))) {
        PRINT("zjs_aio_pin_on: invalid arguments\n");
        return true;
    }

    jerry_object_t *obj = jerry_get_object_value(this_val);

    uint32_t pin;
    zjs_obj_get_uint32(obj, "pin", &pin);

    char event[20];
    jerry_value_t arg = args_p[0];
    jerry_size_t sz = jerry_get_string_size(jerry_get_string_value(arg));
    int len = jerry_string_to_char_buffer(jerry_get_string_value(arg),
                                          (jerry_char_t *)event,
                                          sz);
    event[len] = '\0';

    struct zjs_cb_list_item *item = zjs_aio_get_callback_item(pin, event);
    if (!item) {
        item = zjs_aio_callback_alloc();
    }

    if (!item)
        return false;

    item->pin_obj = obj;
    item->zjs_cb.js_callback = jerry_acquire_object(jerry_get_object_value(args_p[1]));
    item->zjs_cb.call_function = zjs_aio_emit_event;
    memcpy(item->event_type, event, len);

    if (!strcmp(event, "change")) {
        if (jerry_value_is_object(args_p[1])) {
            zjs_aio_ipm_send(TYPE_AIO_PIN_SUBSCRIBE, pin, 0);
        } else {
            zjs_aio_ipm_send(TYPE_AIO_PIN_UNSUBSCRIBE, pin, 0);
        }
    }

    return true;
}

// Asynchrounous Operations
bool zjs_aio_pin_read_async(const jerry_object_t *function_obj_p,
                            const jerry_value_t this_val,
                            const jerry_value_t args_p[],
                            const jerry_length_t args_cnt,
                            jerry_value_t *ret_val_p)
{
    if (args_cnt < 1 || !jerry_value_is_function(args_p[0])) {
        PRINT("zjs_aio_pin_read_async: invalid argument\n");
        return false;
    }

    jerry_object_t *obj = jerry_get_object_value(this_val);
    uint32_t device, pin;
    zjs_obj_get_uint32(obj, "device", &device);
    zjs_obj_get_uint32(obj, "pin", &pin);

    struct zjs_cb_list_item *item = zjs_aio_get_callback_item(pin, NULL);
    if (!item) {
        item = zjs_aio_callback_alloc();
    }

    if (!item)
        return false;

    item->pin_obj = obj;
    item->zjs_cb.js_callback = jerry_get_object_value(args_p[0]);
    item->zjs_cb.call_function = zjs_aio_call_function;

    jerry_acquire_object(item->zjs_cb.js_callback);

    // send IPM message to the ARC side and wait for reponse
    zjs_aio_ipm_send(TYPE_AIO_PIN_READ, pin, 0);
    return true;
}

#endif
