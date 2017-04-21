// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_AIO
#ifndef QEMU_BUILD
// Zephyr includes
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_aio.h"
#include "zjs_callbacks.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_AIO_TIMEOUT_TICKS 5000

const int MAX_TYPE_LEN = 20;

static struct k_sem aio_sem;
static jerry_value_t zjs_aio_prototype;

typedef struct aio_handle {
    zjs_callback_id callback_id;
    double value;
} aio_handle_t;

static aio_handle_t *zjs_aio_alloc_handle()
{
    size_t size = sizeof(aio_handle_t);
    aio_handle_t *handle = zjs_malloc(size);
    if (handle) {
        memset(handle, 0, size);
    }
    return handle;
}

static void zjs_aio_free_cb(void *ptr)
{
    aio_handle_t *handle = (aio_handle_t *)ptr;
    zjs_remove_callback(handle->callback_id);
    zjs_free(handle);
}

static void zjs_aio_free_callback(void *ptr, jerry_value_t rval)
{
    zjs_aio_free_cb(ptr);
}

static const jerry_object_native_info_t aio_type_info =
{
   .free_cb = zjs_aio_free_cb
};

static bool zjs_aio_ipm_send_async(uint32_t type, uint32_t pin, void *data) {
    zjs_ipm_message_t msg;
    msg.id = MSG_ID_AIO;
    msg.flags = 0;
    msg.type = type;
    msg.user_data = data;
    msg.data.aio.pin = pin;
    msg.data.aio.value = 0;

    int success = zjs_ipm_send(MSG_ID_AIO, &msg);
    if (success != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    return true;
}

static bool zjs_aio_ipm_send_sync(zjs_ipm_message_t *send,
                                  zjs_ipm_message_t *result) {
    send->id = MSG_ID_AIO;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_AIO, send) != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (k_sem_take(&aio_sem, ZJS_AIO_TIMEOUT_TICKS)) {
        ERR_PRINT("FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static jerry_value_t zjs_aio_call_remote_function(zjs_ipm_message_t *send)
{
    if (!send)
        return zjs_error("zjs_aio_call_remote_function: invalid send message");

    zjs_ipm_message_t reply;

    bool success = zjs_aio_ipm_send_sync(send, &reply);

    if (!success) {
        return zjs_error("zjs_aio_call_remote_function: ipm message failed or timed out!");
    }

    if (reply.error_code != ERROR_IPM_NONE) {
        ERR_PRINT("error code: %u\n", (unsigned int)reply.error_code);
        return zjs_error("zjs_aio_call_remote_function: error received");
    }

    uint32_t value = reply.data.aio.value;
    return jerry_create_number(value);
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void ipm_msg_receive_callback(void *context, uint32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_AIO)
        return;

    zjs_ipm_message_t *msg = *(zjs_ipm_message_t **)data;

    if (msg->flags & MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;
        // synchronous ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(zjs_ipm_message_t));

        // un-block sync api
        k_sem_give(&aio_sem);
    } else {
        // asynchronous ipm
        aio_handle_t *handle = (aio_handle_t *)msg->user_data;
        uint32_t pin_value = msg->data.aio.value;
#ifdef DEBUG_BUILD
        uint32_t pin = msg->data.aio.pin;
#endif

        switch(msg->type) {
        case TYPE_AIO_PIN_READ:
        case TYPE_AIO_PIN_EVENT_VALUE_CHANGE:
            handle->value = (double)pin_value;
            ZVAL num = jerry_create_number(handle->value);
            zjs_signal_callback(handle->callback_id, &num, sizeof(num));
            break;
        case TYPE_AIO_PIN_SUBSCRIBE:
            DBG_PRINT("subscribed to events on pin %u\n", pin);
            break;
        case TYPE_AIO_PIN_UNSUBSCRIBE:
            DBG_PRINT("unsubscribed to events on pin %u\n", pin);
            break;

        default:
            ERR_PRINT("IPM message not handled %u\n",
                      (unsigned int)msg->type);
        }
    }
}

static ZJS_DECL_FUNC(zjs_aio_pin_read)
{
    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        DBG_PRINT("PIN: #%u\n", pin);
        return zjs_error("zjs_aio_pin_read: pin out of range");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_AIO_PIN_READ;
    send.data.aio.pin = pin;

    jerry_value_t result = zjs_aio_call_remote_function(&send);
    return result;
}

static ZJS_DECL_FUNC(zjs_aio_pin_close)
{
    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    aio_handle_t *handle;
    const jerry_object_native_info_t *tmp;
    if (jerry_get_object_native_pointer(this, (void **)&handle, &tmp)) {
        if (tmp == &aio_type_info) {
            // remove existing onchange handler and unsubscribe
            zjs_aio_ipm_send_async(TYPE_AIO_PIN_UNSUBSCRIBE, pin, handle);
            zjs_remove_callback(handle->callback_id);
            zjs_free(handle);
        }
    }

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_aio_pin_on)
{
    // args: event name, callback
    ZJS_VALIDATE_ARGS(Z_STRING, Z_FUNCTION Z_NULL);

    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    jerry_size_t size = MAX_TYPE_LEN;
    char event[size];
    zjs_copy_jstring(argv[0], event, &size);
    if (strcmp(event, "change"))
        return zjs_error("zjs_aio_pin_on: unsupported event type");

    aio_handle_t *handle;
    const jerry_object_native_info_t *tmp;
    if (jerry_get_object_native_pointer(this, (void **)&handle, &tmp)) {
        if (tmp == &aio_type_info) {
            if (jerry_value_is_null(argv[1])) {
                // no change function, remove if one existed before
                zjs_aio_ipm_send_async(TYPE_AIO_PIN_UNSUBSCRIBE, pin, handle);
                zjs_remove_callback(handle->callback_id);
                zjs_free(handle);
            } else {
                // switch to new change function
                zjs_edit_js_func(handle->callback_id, argv[1]);
            }
        }
    } else if (!jerry_value_is_null(argv[1])) {
        // new change function
        handle = zjs_aio_alloc_handle();
        if (!handle)
            return zjs_error("zjs_aio_pin_on: could not allocate handle");

        jerry_set_object_native_pointer(this, (void *)handle, &aio_type_info);
        handle->callback_id = zjs_add_callback(argv[1], this, handle, NULL);
        zjs_aio_ipm_send_async(TYPE_AIO_PIN_SUBSCRIBE, pin, handle);
    }

    return ZJS_UNDEFINED;
}

// Asynchronous Operations
static ZJS_DECL_FUNC(zjs_aio_pin_read_async)
{
    // args: callback
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    uint32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    aio_handle_t *handle = zjs_aio_alloc_handle();
    if (!handle)
        return zjs_error("zjs_aio_pin_read_async: could not allocate handle");

    handle->callback_id = zjs_add_callback(argv[0], this, handle,
                                           zjs_aio_free_callback);

    jerry_set_object_native_pointer(this, (void *)handle, &aio_type_info);

    // send IPM message to the ARC side; response will come on an ISR
    zjs_aio_ipm_send_async(TYPE_AIO_PIN_READ, pin, handle);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_aio_open)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    jerry_value_t data = argv[0];

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin))
        return zjs_error("zjs_aio_open: missing required field (pin)");

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_AIO_OPEN;
    send.data.aio.pin = pin;

    jerry_value_t result = zjs_aio_call_remote_function(&send);
    if (jerry_value_has_error_flag(result))
        return result;
    jerry_release_value(result);

    // create the AIOPin object
    jerry_value_t pinobj = jerry_create_object();
    jerry_set_prototype(pinobj, zjs_aio_prototype);
    zjs_obj_add_number(pinobj, pin, "pin");

    return pinobj;
}

jerry_value_t zjs_aio_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_AIO, ipm_msg_receive_callback);

    k_sem_init(&aio_sem, 0, 1);

    zjs_native_func_t array[] = {
        { zjs_aio_pin_read, "read" },
        { zjs_aio_pin_read_async, "readAsync" },
        { zjs_aio_pin_close, "close" },
        { zjs_aio_pin_on, "on" },
        { NULL, NULL }
    };
    zjs_aio_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_aio_prototype, array);

    // create global AIO object
    jerry_value_t aio_obj = jerry_create_object();
    zjs_obj_add_function(aio_obj, zjs_aio_open, "open");
    return aio_obj;
}

void zjs_aio_cleanup()
{
    jerry_release_value(zjs_aio_prototype);
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_AIO
