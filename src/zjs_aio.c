// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_AIO
#ifndef QEMU_BUILD
// Zephyr includes
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_aio.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_AIO_TIMEOUT_TICKS                      500

static struct nano_sem aio_sem;

#define MAX_TYPE_LEN 20

struct zjs_cb_list_item {
    char event_type[MAX_TYPE_LEN];  // null-terminated
    jerry_value_t pin_obj;
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
        PRINT("zjs_aio_callback_alloc: out of memory allocating callback struct\n");
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

struct zjs_cb_list_item *zjs_aio_get_callback_item(uint32_t pin,
                                                   const char *event_type)
{
    // calls the JS callback registered in the struct
    struct zjs_cb_list_item **pItem = &zjs_cb_list;
    while (*pItem) {
        jerry_value_t obj = (*pItem)->pin_obj;
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
    jerry_value_t arg = jerry_create_number(mycb->value);
    jerry_value_t rval = jerry_call_function(mycb->zjs_cb.js_callback,
                                             ZJS_UNDEFINED, &arg, 1);
    if (!jerry_value_has_error_flag(rval))
        jerry_release_value(rval);
    jerry_release_value(mycb->zjs_cb.js_callback);
    zjs_aio_callback_free((uintptr_t)mycb);
}

static void zjs_aio_emit_event(struct zjs_callback *cb)
{
    struct zjs_cb_list_item *mycb = CONTAINER_OF(cb, struct zjs_cb_list_item,
                                                 zjs_cb);
    jerry_value_t arg = jerry_create_number(mycb->value);
    jerry_value_t rval = jerry_call_function(mycb->zjs_cb.js_callback,
                                             ZJS_UNDEFINED, &arg, 1);
    if (!jerry_value_has_error_flag(rval))
        jerry_release_value(rval);
}

static struct zjs_ipm_message* zjs_aio_alloc_msg()
{
    struct zjs_ipm_message *msg = task_malloc(sizeof(struct zjs_ipm_message));
    if (!msg) {
        PRINT("zjs_aio_alloc_msg: cannot allocate message\n");
        return NULL;
    } else {
        memset(msg, 0, sizeof(struct zjs_ipm_message));
    }

    msg->id = MSG_ID_AIO;
    msg->flags = 0 | MSG_SAFE_TO_FREE_FLAG;
    msg->error_code = ERROR_IPM_NONE;
    return msg;
}

static void zjs_aio_free_msg(struct zjs_ipm_message* msg)
{
    if (!msg)
        return;

    if (msg->flags & MSG_SAFE_TO_FREE_FLAG) {
        task_free(msg);
    } else {
        PRINT("zjs_aio_free_msg: error! do not free message\n");
    }
}

static bool zjs_aio_ipm_send_async(uint32_t type, uint32_t pin, uint32_t value) {
    struct zjs_ipm_message *msg = zjs_aio_alloc_msg();
    msg->type = type;
    msg->data.aio.pin = pin;
    msg->data.aio.value = value;

    int success = zjs_ipm_send(MSG_ID_AIO, msg);
    if (success != 0) {
        PRINT("zjs_aio_ipm_send: failed to send message\n");
        return false;
    }

    zjs_aio_free_msg(msg);
    return true;
}

static bool zjs_aio_ipm_send_sync(struct zjs_ipm_message* send,
                                  struct zjs_ipm_message* result) {
    send->flags |= MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_AIO, send) != 0) {
        PRINT("zjs_aio_ipm_send_sync: failed to send message\n");
        return false;
    }

    // block until reply or timeout
    if (!nano_sem_take(&aio_sem, ZJS_AIO_TIMEOUT_TICKS)) {
        PRINT("zjs_aio_ipm_send_sync: ipm timed out\n");
        return false;
    }

    return true;
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_AIO)
        return;

    struct zjs_ipm_message *msg = (struct zjs_ipm_message*)(*(uintptr_t *)data);

    if (msg->flags & MSG_SYNC_FLAG) {
         struct zjs_ipm_message *result = (struct zjs_ipm_message*)msg->user_data;
        // synchronous ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(struct zjs_ipm_message));

        // un-block sync api
        nano_isr_sem_give(&aio_sem);
    } else {
        // asynchronous ipm
        uint32_t pin = msg->data.aio.pin;
        uint32_t pin_value = msg->data.aio.value;
        struct zjs_cb_list_item *mycb;

        switch(msg->type) {
        case TYPE_AIO_PIN_READ:
            mycb = zjs_aio_get_callback_item(pin, NULL);
            if (mycb && mycb->zjs_cb.js_callback) {
                // TODO: ensure that there is no outstanding callback of this
                //   type or else we may be overwriting the value it should have
                //   reported
                mycb->value = (double)pin_value;
                zjs_queue_callback(&mycb->zjs_cb);
            } else {
                PRINT("ipm_msg_receive_callback: callback not found\n");
            }
            break;
        case TYPE_AIO_PIN_SUBSCRIBE:
            PRINT("ipm_msg_receive_callback: subscribed to events on pin %lu\n", pin);
            break;
        case TYPE_AIO_PIN_UNSUBSCRIBE:
            PRINT("ipm_msg_receive_callback: unsubscribed to events on pin %lu\n", pin);
            break;
        case TYPE_AIO_PIN_EVENT_VALUE_CHANGE:
        {
            mycb = zjs_aio_get_callback_item(pin, "change");
            if (mycb && mycb->zjs_cb.js_callback) {
                mycb->value = (double)pin_value;
                zjs_queue_callback(&mycb->zjs_cb);
            } else {
                PRINT("ipm_msg_receive_callback: onChange event callback not found\n");
            }
            break;
        }

        default:
            PRINT("ipm_msg_receive_callback: IPM message not handled %lu\n", msg->type);
        }
    }
}

static jerry_value_t zjs_aio_pin_read(const jerry_value_t function_obj_val,
                                      const jerry_value_t this_val,
                                      const jerry_value_t args_p[],
                                      const jerry_length_t args_cnt)
{
    uint32_t device, pin;
    zjs_obj_get_uint32(this_val, "device", &device);
    zjs_obj_get_uint32(this_val, "pin", &pin);

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        PRINT("PIN: #%lu\n", pin);
        return zjs_error("zjs_aio_pin_read: pin out of range");
    }

    // send IPM message to the ARC side
    struct zjs_ipm_message* reply = zjs_aio_alloc_msg();;
    struct zjs_ipm_message* send = zjs_aio_alloc_msg();

    send->type = TYPE_AIO_PIN_READ;
    send->data.aio.pin = pin;
    bool success = zjs_aio_ipm_send_sync(send, reply);
    zjs_aio_free_msg(send);

    if (!success) {
        zjs_aio_free_msg(reply);
        return zjs_error("zjs_aio_pin_read: ipm message failed or timed out!");
    }

    if (reply->error_code != ERROR_IPM_NONE) {
        PRINT("error code: %lu\n", reply->error_code);
        zjs_aio_free_msg(reply);
        return zjs_error("zjs_aio_pin_read: error received");
    }

    double value = reply->data.aio.value;
    zjs_aio_free_msg(reply);

    return jerry_create_number(value);
}

static jerry_value_t zjs_aio_pin_abort(const jerry_value_t function_obj_val,
                                       const jerry_value_t this_val,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_cnt)
{
    // NO-OP
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_aio_pin_close(const jerry_value_t function_obj_val,
                                       const jerry_value_t this_val,
                                       const jerry_value_t args_p[],
                                       const jerry_length_t args_cnt)
{
    // NO-OP
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_aio_pin_on(const jerry_value_t function_obj_val,
                                    const jerry_value_t this_val,
                                    const jerry_value_t args_p[],
                                    const jerry_length_t args_cnt)
{
    if (args_cnt < 2 ||
        !jerry_value_is_string(args_p[0]) ||
        (!jerry_value_is_object(args_p[1]) &&
         !jerry_value_is_null(args_p[1]))) {
        return zjs_error("zjs_aio_pin_on: invalid argument");
    }

    uint32_t pin;
    zjs_obj_get_uint32(this_val, "pin", &pin);

    char event[MAX_TYPE_LEN];
    jerry_value_t arg = args_p[0];
    jerry_size_t sz = jerry_get_string_size(arg);
    if (sz >= MAX_TYPE_LEN)
        return zjs_error("zjs_aio_pin_on: event string too long");
    int len = jerry_string_to_char_buffer(arg, (jerry_char_t *)event, sz);
    event[len] = '\0';

    struct zjs_cb_list_item *item = zjs_aio_get_callback_item(pin, event);
    if (!item) {
        item = zjs_aio_callback_alloc();
    }

    if (!item)
        return zjs_error("zjs_aio_pin_on: callback item not available");

    item->pin_obj = this_val;
    item->zjs_cb.js_callback = jerry_acquire_value(args_p[1]);
    item->zjs_cb.call_function = zjs_aio_emit_event;
    memcpy(item->event_type, event, len + 1);

    if (!strcmp(event, "change")) {
        if (jerry_value_is_object(args_p[1])) {
            zjs_aio_ipm_send_async(TYPE_AIO_PIN_SUBSCRIBE, pin, 0);
        } else {
            zjs_aio_ipm_send_async(TYPE_AIO_PIN_UNSUBSCRIBE, pin, 0);
        }
    }

    return ZJS_UNDEFINED;
}

// Asynchronous Operations
static jerry_value_t zjs_aio_pin_read_async(const jerry_value_t function_obj_val,
                                            const jerry_value_t this_val,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_cnt)
{
    if (args_cnt < 1 || !jerry_value_is_function(args_p[0]))
        return zjs_error("zjs_aio_pin_read_async: invalid argument");

    uint32_t device, pin;
    zjs_obj_get_uint32(this_val, "device", &device);
    zjs_obj_get_uint32(this_val, "pin", &pin);

    struct zjs_cb_list_item *item = zjs_aio_get_callback_item(pin, NULL);
    if (!item) {
        item = zjs_aio_callback_alloc();
    }

    if (!item)
        return zjs_error("zjs_aio_pin_read_async: callback item not available");

    item->pin_obj = this_val;
    item->zjs_cb.js_callback = jerry_acquire_value(args_p[0]);
    item->zjs_cb.call_function = zjs_aio_call_function;

    // send IPM message to the ARC side and wait for reponse
    zjs_aio_ipm_send_async(TYPE_AIO_PIN_READ, pin, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_aio_open(const jerry_value_t function_obj_val,
                                  const jerry_value_t this_val,
                                  const jerry_value_t args_p[],
                                  const jerry_length_t args_cnt)
{
    if (args_cnt < 1 || !jerry_value_is_object(args_p[0]))
        return zjs_error("zjs_aio_open: invalid argument");

    jerry_value_t data = args_p[0];

    uint32_t device;
    if (!zjs_obj_get_uint32(data, "device", &device))
        return zjs_error("zjs_aio_open: missing required field (device)");

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin))
        return zjs_error("zjs_aio_open: missing required field (pin)");

    const int BUFLEN = 32;
    char buffer[BUFLEN];

    if (zjs_obj_get_string(data, "name", buffer, BUFLEN)) {
        buffer[0] = '\0';
    }

    char *name = buffer;
    bool raw = false;
    zjs_obj_get_boolean(data, "raw", &raw);

    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_aio_alloc_msg();
    struct zjs_ipm_message* reply = zjs_aio_alloc_msg();

    send->type = TYPE_AIO_OPEN;
    send->data.aio.pin = pin;
    bool success = zjs_aio_ipm_send_sync(send, reply);
    zjs_aio_free_msg(send);

    if (!success) {
        zjs_aio_free_msg(reply);
        return zjs_error("zjs_aio_open: ipm message failed or timed out!");
    }

    if (reply->error_code != ERROR_IPM_NONE) {
        PRINT("error code: %lu\n", reply->error_code);
        zjs_aio_free_msg(reply);
        return zjs_error("zjs_aio_open: error received");
    }

    zjs_aio_free_msg(send);
    zjs_aio_free_msg(reply);

    // create the AIOPin object
    jerry_value_t pinobj = jerry_create_object();
    zjs_obj_add_function(pinobj, zjs_aio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_aio_pin_read_async, "readAsync");
    zjs_obj_add_function(pinobj, zjs_aio_pin_abort, "abort");
    zjs_obj_add_function(pinobj, zjs_aio_pin_close, "close");
    zjs_obj_add_function(pinobj, zjs_aio_pin_on, "on");
    zjs_obj_add_string(pinobj, name, "name");
    zjs_obj_add_number(pinobj, device, "device");
    zjs_obj_add_number(pinobj, pin, "pin");
    zjs_obj_add_boolean(pinobj, raw, "raw");

    return pinobj;
}

jerry_value_t zjs_aio_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(ipm_msg_receive_callback);

    nano_sem_init(&aio_sem);

    // create global AIO object
    jerry_value_t aio_obj = jerry_create_object();
    zjs_obj_add_function(aio_obj, zjs_aio_open, "open");
    return aio_obj;
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_AIO
