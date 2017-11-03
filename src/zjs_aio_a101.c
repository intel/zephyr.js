// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_AIO
#ifndef QEMU_BUILD
// C includes
#include <string.h>

// Zephyr includes
#include <misc/util.h>

// ZJS includes
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_AIO_TIMEOUT_TICKS 5000

const int MAX_TYPE_LEN = 20;

static struct k_sem aio_sem;
static jerry_value_t zjs_aio_prototype;

typedef struct aio_handle {
    jerry_value_t pin_obj;
    u32_t pin;
} aio_handle_t;

// get the aio handle or return a JS error
#define GET_AIO_HANDLE(obj, var)                                     \
aio_handle_t *var = (aio_handle_t *)zjs_event_get_user_handle(obj);  \
if (!var) { return zjs_error("no aio handle"); }

static bool zjs_aio_ipm_send_async(u32_t type, u32_t pin, void *data) {
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
                                  zjs_ipm_message_t *result)
{
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

static aio_handle_t *aio_alloc_handle()
{
    size_t size = sizeof(aio_handle_t);
    aio_handle_t *handle = zjs_malloc(size);
    if (handle) {
        memset(handle, 0, size);
    }
    return handle;
}

static void aio_free_cb(void *ptr)
{
    aio_handle_t *handle = (aio_handle_t *)ptr;

    // unsubscribe
    zjs_aio_ipm_send_async(TYPE_AIO_PIN_UNSUBSCRIBE, handle->pin, handle);
    jerry_release_value(handle->pin_obj);
    zjs_free(handle);
}

static jerry_value_t zjs_aio_call_remote_function(zjs_ipm_message_t *send)
{
    if (!send)
        return zjs_error_context("invalid send message", 0, 0);

    zjs_ipm_message_t reply;

    bool success = zjs_aio_ipm_send_sync(send, &reply);

    if (!success) {
        return zjs_error_context("ipm message failed or timed out!", 0, 0);
    }

    if (reply.error_code != ERROR_IPM_NONE) {
        ERR_PRINT("error code: %u\n", (unsigned int)reply.error_code);
        return zjs_error_context("error received", 0, 0);
    }

    u32_t value = reply.data.aio.value;
    return jerry_create_number(value);
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void ipm_msg_receive_callback(void *context, u32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_AIO)
        return;

    zjs_ipm_message_t *msg = *(zjs_ipm_message_t **)data;

    if (msg->flags & MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;

        // synchronous ipm, copy the results
        if (result) {
            *result = *msg;
        }

        // un-block sync api
        k_sem_give(&aio_sem);
    } else {
        // asynchronous ipm
        aio_handle_t *handle = (aio_handle_t *)msg->user_data;
        u32_t pin_value = msg->data.aio.value;
#ifdef DEBUG_BUILD
        u32_t pin = msg->data.aio.pin;
#endif
        ZVAL num = jerry_create_number(pin_value);

        switch (msg->type) {
        case TYPE_AIO_PIN_READ:
            DBG_PRINT("aio async read %d\n", pin_value);
            break;
        case TYPE_AIO_PIN_EVENT_VALUE_CHANGE:
            zjs_defer_emit_event(handle->pin_obj, "change", &num,
                                 sizeof(num), zjs_copy_arg, zjs_release_args);
            break;
        case TYPE_AIO_PIN_SUBSCRIBE:
            DBG_PRINT("subscribed to events on pin %u\n", pin);
            break;
        case TYPE_AIO_PIN_UNSUBSCRIBE:
            DBG_PRINT("unsubscribed to events on pin %u\n", pin);
            break;

        default:
            ERR_PRINT("IPM message not handled %u\n", (unsigned int)msg->type);
        }
    }
}

static jerry_value_t aio_pin_read(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc,
                                  bool async)
{
    u32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);

    if (pin < AIO_MIN || pin > AIO_MAX) {
        DBG_PRINT("PIN: #%u\n", pin);
        return zjs_error("pin out of range");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_AIO_PIN_READ;
    send.data.aio.pin = pin;

    jerry_value_t result = zjs_aio_call_remote_function(&send);

    if (async) {
#ifdef ZJS_FIND_FUNC_NAME
        zjs_obj_add_string(argv[0], ZJS_HIDDEN_PROP("function_name"), "readAsync");
#endif
        zjs_callback_id id = zjs_add_callback_once(argv[0], this, NULL, NULL);
        zjs_signal_callback(id, &result, sizeof(result));
        return ZJS_UNDEFINED;
    } else {
        return result;
    }
}

static ZJS_DECL_FUNC(zjs_aio_pin_read)
{
    return aio_pin_read(function_obj,
                        this,
                        argv,
                        argc,
                        false);
}

// Asynchronous Operations
static ZJS_DECL_FUNC(zjs_aio_pin_read_async)
{
    // args: callback
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    return aio_pin_read(function_obj,
                        this,
                        argv,
                        argc,
                        true);
}

static ZJS_DECL_FUNC(zjs_aio_pin_close)
{
    u32_t pin;
    zjs_obj_get_uint32(this, "pin", &pin);
    GET_AIO_HANDLE(this, handle);

    aio_free_cb(handle);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_aio_open)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    jerry_value_t data = argv[0];

    u32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin))
        return zjs_error("missing required field (pin)");

    if (pin < AIO_MIN || pin > AIO_MAX) {
        DBG_PRINT("PIN: #%u\n", pin);
        return zjs_error("pin out of range");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_AIO_OPEN;
    send.data.aio.pin = pin;

    jerry_value_t result = zjs_aio_call_remote_function(&send);
    if (jerry_value_has_error_flag(result))
        return result;
    jerry_release_value(result);

    // create the AIOPin object
    jerry_value_t pinobj = zjs_create_object();
    jerry_set_prototype(pinobj, zjs_aio_prototype);
    zjs_obj_add_number(pinobj, "pin", pin);

    aio_handle_t *handle = aio_alloc_handle();
    if (!handle) {
        return zjs_error("could not allocate handle");
    }

    handle->pin_obj = jerry_acquire_value(pinobj);
    handle->pin = pin;

    // make it an emitter object and subscribe to changes
    zjs_make_emitter(pinobj, zjs_aio_prototype, handle, aio_free_cb);
    zjs_aio_ipm_send_async(TYPE_AIO_PIN_SUBSCRIBE, pin, handle);

    return pinobj;
}

static void zjs_aio_cleanup(void *native)
{
    jerry_release_value(zjs_aio_prototype);
}

static const jerry_object_native_info_t aio_module_type_info = {
   .free_cb = zjs_aio_cleanup
};

jerry_value_t zjs_aio_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_AIO, ipm_msg_receive_callback);

    k_sem_init(&aio_sem, 0, 1);

    zjs_native_func_t array[] = {
        { zjs_aio_pin_read, "read" },
        { zjs_aio_pin_read_async, "readAsync" },
        { zjs_aio_pin_close, "close" },
        { NULL, NULL }
    };

    zjs_aio_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_aio_prototype, array);

    // create global AIO object
    jerry_value_t aio_obj = zjs_create_object();
    zjs_obj_add_function(aio_obj, "open", zjs_aio_open);
    jerry_set_object_native_pointer(aio_obj, NULL, &aio_module_type_info);
    return aio_obj;
}

JERRYX_NATIVE_MODULE(aio, zjs_aio_init)
#endif  // QEMU_BUILD
#endif  // BUILD_MODULE_AIO
