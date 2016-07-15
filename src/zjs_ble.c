// Copyright (c) 2016, Intel Corporation.
#ifndef QEMU_BUILD
// Zephyr includes
#include <zephyr.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

// ZJS includes
#include "zjs_ble.h"
#include "zjs_buffer.h"
#include "zjs_util.h"

#define ZJS_BLE_UUID_LEN                            36

#define ZJS_BLE_RESULT_SUCCESS                      0x00
#define ZJS_BLE_RESULT_INVALID_OFFSET               BT_ATT_ERR_INVALID_OFFSET
#define ZJS_BLE_RESULT_ATTR_NOT_LONG                BT_ATT_ERR_ATTRIBUTE_NOT_LONG
#define ZJS_BLE_RESULT_INVALID_ATTRIBUTE_LENGTH     BT_ATT_ERR_INVALID_ATTRIBUTE_LEN
#define ZJS_BLE_RESULT_UNLIKELY_ERROR               BT_ATT_ERR_UNLIKELY

// Port this to javascript
#define BT_UUID_WEBBT BT_UUID_DECLARE_16(0xfc00)
#define BT_UUID_TEMP BT_UUID_DECLARE_16(0xfc0a)
#define BT_UUID_RGB BT_UUID_DECLARE_16(0xfc0b)
#define SENSOR_1_NAME "Temperature"
#define SENSOR_2_NAME "Led"

struct nano_sem zjs_ble_nano_sem;

static struct bt_gatt_ccc_cfg zjs_ble_blvl_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
static uint8_t zjs_ble_simulate_blvl;

struct bt_conn *zjs_ble_default_conn;

struct zjs_ble_read_callback {
    struct zjs_callback zjs_cb;
    uint16_t offset;                        // arg
    uint32_t error_code;                    // return value
    const void *buffer;                     // return value
    ssize_t buffer_size;                    // return value
};

struct zjs_ble_write_callback {
    struct zjs_callback zjs_cb;
    const void *buffer;                     // arg
    uint16_t buffer_size;                   // arg
    uint16_t offset;                        // arg
    uint32_t error_code;                    // return value
};

struct zjs_ble_subscribe_callback {
    struct zjs_callback zjs_cb;
    uint16_t max_value_size;
};

struct zjs_ble_unsubscribe_callback {
    struct zjs_callback zjs_cb;
    // placeholders args
};

struct zjs_ble_notify_callback {
    struct zjs_callback zjs_cb;
    // placeholders args
};

struct zjs_ble_characteristic {
    int flags;
    jerry_value_t chrc_obj;
    struct bt_uuid *uuid;
    struct bt_gatt_attr *chrc_attr;
    struct zjs_ble_read_callback read_cb;
    struct zjs_ble_write_callback write_cb;
    struct zjs_ble_subscribe_callback subscribe_cb;
    struct zjs_ble_unsubscribe_callback unsubscribe_cb;
    struct zjs_ble_notify_callback notify_cb;
    struct zjs_ble_characteristic *next;
};

struct zjs_ble_service {
    jerry_value_t service_obj;
    struct bt_uuid *uuid;
    struct zjs_ble_characteristic *characteristics;
    struct zjs_ble_service *next;
};

struct zjs_ble_list_item {
    char event_type[20];
    struct zjs_callback zjs_cb;
    uint32_t intdata;
    struct zjs_ble_list_item *next;
};

static struct zjs_ble_service zjs_ble_service = { NULL, NULL, NULL };
static struct zjs_ble_list_item *zjs_ble_list = NULL;

struct bt_uuid* zjs_ble_new_uuid_16(uint16_t value) {
    struct bt_uuid_16* uuid = task_malloc(sizeof(struct bt_uuid_16));
    if (!uuid) {
        PRINT("zjs_ble_new_uuid_16: out of memory allocating struct bt_uuid_16\n");
        return NULL;
    } else {
        memset(uuid, 0, sizeof(struct bt_uuid_16));
    }

    uuid->uuid.type = BT_UUID_TYPE_16;
    uuid->val = value;
    return (struct bt_uuid *) uuid;
}

static void zjs_ble_free_characteristics(struct zjs_ble_characteristic *chrc)
{
    struct zjs_ble_characteristic *tmp;
    while (chrc != NULL) {
        tmp = chrc;
        chrc = chrc->next;

        jerry_release_value(chrc->chrc_obj);

        if (tmp->read_cb.zjs_cb.js_callback)
            jerry_release_value(tmp->read_cb.zjs_cb.js_callback);
        if (tmp->write_cb.zjs_cb.js_callback)
            jerry_release_value(tmp->write_cb.zjs_cb.js_callback);
        if (tmp->subscribe_cb.zjs_cb.js_callback)
            jerry_release_value(tmp->subscribe_cb.zjs_cb.js_callback);
        if (tmp->unsubscribe_cb.zjs_cb.js_callback)
            jerry_release_value(tmp->unsubscribe_cb.zjs_cb.js_callback);
        if (tmp->notify_cb.zjs_cb.js_callback)
            jerry_release_value(tmp->notify_cb.zjs_cb.js_callback);

        task_free(tmp);
    }
}

static struct zjs_ble_list_item *zjs_ble_event_callback_alloc()
{
    // effects: allocates a new callback list item and adds it to the list
    struct zjs_ble_list_item *item;
    item = task_malloc(sizeof(struct zjs_ble_list_item));
    if (!item) {
        PRINT("zjs_ble_event_callback_alloc: out of memory allocating callback struct\n");
        return NULL;
    }

    item->next = zjs_ble_list;
    zjs_ble_list = item;
    return item;
}

static void zjs_ble_queue_dispatch(char *type, zjs_cb_wrapper_t func,
                                   uint32_t intdata)
{
    // requires: called only from task context, type is the string event type,
    //             func is a function that can handle calling the callback for
    //             this event type when found, intarg is a uint32 that will be
    //             stored in the appropriate callback struct for use by func
    //             (just set it to 0 if not needed)
    //  effects: finds the first callback for the given type and queues it up
    //             to run func to execute it at the next opportunity
    struct zjs_ble_list_item *ev = zjs_ble_list;
    int len = strlen(type);
    while (ev) {
        if (!strncmp(ev->event_type, type, len)) {
            ev->zjs_cb.call_function = func;
            ev->intdata = intdata;
            zjs_queue_callback(&ev->zjs_cb);
            return;
        }
        ev = ev->next;
    }
}

static jerry_value_t zjs_ble_read_attr_call_function_return(const jerry_value_t function_obj_val,
                                                            const jerry_value_t this_val,
                                                            const jerry_value_t args_p[],
                                                            const jerry_length_t args_cnt)
{
    if (args_cnt != 2 ||
        !jerry_value_is_number(args_p[0]) ||
        !jerry_value_is_object(args_p[1])) {
        PRINT("zjs_ble_read_attr_call_function_return: invalid arguments\n");
        nano_task_sem_give(&zjs_ble_nano_sem);
        return ZJS_UNDEFINED;
    }

    uintptr_t ptr;
    if (jerry_get_object_native_handle(function_obj_val, &ptr)) {
        // store the return value in the read_cb struct
        struct zjs_ble_characteristic *chrc = (struct zjs_ble_characteristic*)ptr;
        chrc->read_cb.error_code = (uint32_t)jerry_get_number_value(args_p[0]);


        struct zjs_buffer_t *buf = zjs_buffer_find(args_p[1]);
        if (buf) {
            chrc->read_cb.buffer = buf->buffer;
            chrc->read_cb.buffer_size = buf->bufsize;
        } else {
            PRINT("zjs_ble_read_attr_call_function_return: buffer not found\n");
        }
    }

    // unblock fiber
    nano_task_sem_give(&zjs_ble_nano_sem);
    return ZJS_UNDEFINED;
}

static void zjs_ble_read_attr_call_function(struct zjs_callback *cb)
{
    struct zjs_ble_read_callback *mycb;
    struct zjs_ble_characteristic *chrc;
    mycb = CONTAINER_OF(cb, struct zjs_ble_read_callback, zjs_cb);
    chrc = CONTAINER_OF(mycb, struct zjs_ble_characteristic, read_cb);

    jerry_value_t rval;
    jerry_value_t args[2];
    jerry_value_t func_obj;

    args[0] = jerry_create_number(mycb->offset);
    func_obj = jerry_create_external_function(zjs_ble_read_attr_call_function_return);
    jerry_set_object_native_handle(func_obj, (uintptr_t)chrc, NULL);
    args[1] = func_obj;

    rval = jerry_call_function(mycb->zjs_cb.js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_read_attr_call_function: failed to call onReadRequest function\n");
    }

    jerry_release_value(args[0]);
    jerry_release_value(args[1]);
    jerry_release_value(rval);
}

static ssize_t zjs_ble_read_attr_callback(struct bt_conn *conn,
                                          const struct bt_gatt_attr *attr,
                                          void *buf, uint16_t len,
                                          uint16_t offset)
{
    if (offset > len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    struct zjs_ble_characteristic* chrc = attr->user_data;

    if (!chrc) {
        PRINT("zjs_ble_read_attr_callback: characteristic not found\n");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
    }

    if (chrc->read_cb.zjs_cb.js_callback) {
        // This is from the FIBER context, so we queue up the callback
        // to invoke js from task context
        chrc->read_cb.offset = offset;
        chrc->read_cb.buffer = NULL;
        chrc->read_cb.buffer_size = 0;
        chrc->read_cb.error_code = BT_ATT_ERR_NOT_SUPPORTED;
        chrc->read_cb.zjs_cb.call_function = zjs_ble_read_attr_call_function;
        zjs_queue_callback(&chrc->read_cb.zjs_cb);

        // block until result is ready
        nano_fiber_sem_take(&zjs_ble_nano_sem, TICKS_UNLIMITED);

        if (chrc->read_cb.error_code == ZJS_BLE_RESULT_SUCCESS) {
            if (chrc->read_cb.buffer && chrc->read_cb.buffer_size > 0) {
                // buffer should be pointing to the Buffer object that JS created
                // copy the bytes into the return buffer ptr
                memcpy(buf, chrc->read_cb.buffer, chrc->read_cb.buffer_size);
                return chrc->read_cb.buffer_size;
            }

            PRINT("zjs_ble_read_attr_callback: buffer is empty\n");
            return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
        } else {
            PRINT("zjs_ble_read_attr_callback: on read attr error %lu\n", chrc->read_cb.error_code);
            return BT_GATT_ERR(chrc->read_cb.error_code);
        }
    }

    PRINT("zjs_ble_read_attr_callback: js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static jerry_value_t zjs_ble_write_attr_call_function_return(const jerry_value_t function_obj_val,
                                                             const jerry_value_t this_val,
                                                             const jerry_value_t args_p[],
                                                             const jerry_length_t args_cnt)
{
    if (args_cnt != 1 ||
        !jerry_value_is_number(args_p[0])) {
        PRINT("zjs_ble_write_attr_call_function_return: invalid arguments\n");
        nano_task_sem_give(&zjs_ble_nano_sem);
        return true;
    }

    uintptr_t ptr;
    if (jerry_get_object_native_handle(function_obj_val, &ptr)) {
        // store the return value in the write_cb struct
        struct zjs_ble_characteristic *chrc = (struct zjs_ble_characteristic*)ptr;
        chrc->write_cb.error_code = (uint32_t)jerry_get_number_value(args_p[0]);
    }

    // unblock fiber
    nano_task_sem_give(&zjs_ble_nano_sem);
    return true;
}

static void zjs_ble_write_attr_call_function(struct zjs_callback *cb)
{
    struct zjs_ble_write_callback *mycb;
    struct zjs_ble_characteristic *chrc;
    mycb = CONTAINER_OF(cb, struct zjs_ble_write_callback, zjs_cb);
    chrc = CONTAINER_OF(mycb, struct zjs_ble_characteristic, write_cb);

    jerry_value_t rval;
    jerry_value_t args[4];
    jerry_value_t func_obj;

    if (mycb->buffer && mycb->buffer_size > 0) {
        jerry_value_t buf_obj = zjs_buffer_create(mycb->buffer_size);

        if (buf_obj) {
           struct zjs_buffer_t *buf = zjs_buffer_find(buf_obj);

           if (buf &&
               buf->buffer &&
               buf->bufsize == mycb->buffer_size) {
               memcpy(buf->buffer, mycb->buffer, mycb->buffer_size);
               args[0]= buf_obj;
           } else {
               args[0] = jerry_create_null();
           }
        } else {
            args[0] = jerry_create_null();
        }
    } else {
        args[0] = jerry_create_null();
    }

    args[1] = jerry_create_number(mycb->offset);
    args[2] = jerry_create_boolean(false);
    func_obj = jerry_create_external_function(zjs_ble_write_attr_call_function_return);
    jerry_set_object_native_handle(func_obj, (uintptr_t)chrc, NULL);
    args[3] = func_obj;

    rval = jerry_call_function(mycb->zjs_cb.js_callback, chrc->chrc_obj, args, 4);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_write_attr_call_function: failed to call onWriteRequest function\n");
    }

    jerry_release_value(args[0]);
    jerry_release_value(args[1]);
    jerry_release_value(args[2]);
    jerry_release_value(args[3]);
    jerry_release_value(rval);
}

static ssize_t zjs_ble_write_attr_callback(struct bt_conn *conn,
                                           const struct bt_gatt_attr *attr,
                                           const void *buf, uint16_t len,
                                           uint16_t offset, uint8_t flags)
{
    struct zjs_ble_characteristic* chrc = attr->user_data;

    if (!chrc) {
        PRINT("zjs_ble_write_attr_callback: characteristic not found\n");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
    }

    if (chrc->write_cb.zjs_cb.js_callback) {
        // This is from the FIBER context, so we queue up the callback
        // to invoke js from task context
        chrc->write_cb.offset = offset;
        chrc->write_cb.buffer = (len > 0) ? buf : NULL;
        chrc->write_cb.buffer_size = len;
        chrc->write_cb.error_code = BT_ATT_ERR_NOT_SUPPORTED;
        chrc->write_cb.zjs_cb.call_function = zjs_ble_write_attr_call_function;
        zjs_queue_callback(&chrc->write_cb.zjs_cb);

        // block until result is ready
        nano_fiber_sem_take(&zjs_ble_nano_sem, TICKS_UNLIMITED);

        if (chrc->write_cb.error_code == ZJS_BLE_RESULT_SUCCESS) {
            return len;
        } else {
            return BT_GATT_ERR(chrc->write_cb.error_code);
        }
    }

    PRINT("zjs_ble_write_attr_callback: js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static jerry_value_t zjs_ble_update_value_call_function(const jerry_value_t function_obj_val,
                                                        const jerry_value_t this_val,
                                                        const jerry_value_t args_p[],
                                                        const jerry_length_t args_cnt)
{
    if (args_cnt != 1 ||
        !jerry_value_is_object(args_p[0])) {
        PRINT("zjs_ble_update_value_call_function: invalid arguments\n");
        return false;
    }

    // expects a Buffer object
    struct zjs_buffer_t *buf = zjs_buffer_find(args_p[0]);

    if (buf) {
        if (zjs_ble_default_conn) {
            uintptr_t ptr;
            if (jerry_get_object_native_handle(this_val, &ptr)) {
               struct zjs_ble_characteristic *chrc = (struct zjs_ble_characteristic*)ptr;
               if (chrc->chrc_attr) {
                   bt_gatt_notify(zjs_ble_default_conn, chrc->chrc_attr, buf->buffer, buf->bufsize);
               }
            }
        }

        return true;
    }

    PRINT("updateValueCallback: buffer not found or empty\n");
    return false;
}

static void zjs_ble_subscribe_call_function(struct zjs_callback *cb)
{
    struct zjs_ble_subscribe_callback *mycb = CONTAINER_OF(cb,
                                                           struct zjs_ble_subscribe_callback,
                                                           zjs_cb);
    struct zjs_ble_characteristic *chrc = CONTAINER_OF(mycb,
                                                       struct zjs_ble_characteristic,
                                                       subscribe_cb);

    jerry_value_t rval;
    jerry_value_t args[2];

    args[0] = jerry_create_number(20); // max payload size
    args[1] = jerry_create_external_function(zjs_ble_update_value_call_function);
    rval = jerry_call_function(mycb->zjs_cb.js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_subscribe_call_function: failed to call onSubscribe function\n");
    }

    jerry_release_value(args[0]);
    jerry_release_value(args[1]);
    jerry_release_value(rval);
}

// Port this to javascript
static void zjs_ble_blvl_ccc_cfg_changed(uint16_t value)
{
    zjs_ble_simulate_blvl = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void zjs_ble_accept_call_function(struct zjs_callback *cb)
{
    // FIXME: get real bluetooth address
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"AB:CD:DF:AB:CD:EF");
    jerry_value_t rval = jerry_call_function(cb->js_callback, ZJS_UNDEFINED, &arg, 1);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_accept_call_function: failed to call function\n");
    }
    jerry_release_value(rval);
    jerry_release_value(arg);
}

static void zjs_ble_disconnect_call_function(struct zjs_callback *cb)
{
    // FIXME: get real bluetooth address
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"AB:CD:DF:AB:CD:EF");
    jerry_value_t rval = jerry_call_function(cb->js_callback, ZJS_UNDEFINED, &arg, 1);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_disconnect_call_function: failed to call function\n");
    }
    jerry_release_value(rval);
    jerry_release_value(arg);
}

static void zjs_ble_connected(struct bt_conn *conn, uint8_t err)
{
    PRINT("========= connected ========\n");
    if (err) {
        PRINT("zjs_ble_connected: Connection failed (err %u)\n", err);
    } else {
        zjs_ble_default_conn = bt_conn_ref(conn);
        PRINT("Connected\n");
        // FIXME: temporary fix for BLE bug
        fiber_sleep(100);
        zjs_ble_queue_dispatch("accept", zjs_ble_accept_call_function, 0);
    }
}

static void zjs_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
    PRINT("Disconnected (reason %u)\n", reason);

    if (zjs_ble_default_conn) {
        bt_conn_unref(zjs_ble_default_conn);
        zjs_ble_default_conn = NULL;
        zjs_ble_queue_dispatch("disconnect", zjs_ble_disconnect_call_function, 0);
    }
}

static struct bt_conn_cb zjs_ble_conn_callbacks = {
    .connected = zjs_ble_connected,
    .disconnected = zjs_ble_disconnected,
};

static void zjs_ble_auth_cancel(struct bt_conn *conn)
{
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        PRINT("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb zjs_ble_auth_cb_display = {
        .cancel = zjs_ble_auth_cancel,
};

static void zjs_ble_bt_ready_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context
    //  effects: handles execution of the bt ready JS callback
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"poweredOn");
    jerry_value_t rval = jerry_call_function(cb->js_callback, ZJS_UNDEFINED, &arg, 1);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_bt_ready_call_function: failed to call function\n");
    }
    jerry_release_value(rval);
    jerry_release_value(arg);
}

static void zjs_ble_bt_ready(int err)
{
    if (!zjs_ble_list) {
        PRINT("zjs_ble_bt_ready: no event handlers present\n");
        return;
    }
    PRINT("zjs_ble_bt_ready is called [err %d]\n", err);

    // FIXME: Probably we should return this err to JS like in adv_start?
    //   Maybe this wasn't in the bleno API?
    zjs_ble_queue_dispatch("stateChange", zjs_ble_bt_ready_call_function, 0);
}

void zjs_ble_enable() {
    PRINT("About to enable the bluetooth, wait for bt_ready()...\n");
    bt_enable(zjs_ble_bt_ready);
    // setup connection callbacks
    bt_conn_cb_register(&zjs_ble_conn_callbacks);
    bt_conn_auth_cb_register(&zjs_ble_auth_cb_display);
}

static jerry_value_t zjs_ble_on(const jerry_value_t function_obj_val,
                                const jerry_value_t this_val,
                                const jerry_value_t args_p[],
                                const jerry_length_t args_cnt)
{
    if (args_cnt < 2 ||
        !jerry_value_is_string(args_p[0]) ||
        !jerry_value_is_object(args_p[1])) {
        PRINT("zjs_ble_on: invalid arguments\n");
        return false;
    }

    struct zjs_ble_list_item *item = zjs_ble_event_callback_alloc();
    if (!item)
        return false;

    char event[20];
    jerry_size_t sz = jerry_get_string_size(args_p[0]);
    // FIXME: need to make sure event name is less than 20 characters.
    // assert(sz < 20);
    int len = jerry_string_to_char_buffer(args_p[0],
                                          (jerry_char_t *)event,
                                          sz);
    event[len] = '\0';
    PRINT("\nEVENT TYPE: %s (%d)\n", event, len);

    item->zjs_cb.js_callback = jerry_acquire_value(args_p[1]);
    memcpy(item->event_type, event, len);

    return true;
}

static void zjs_ble_adv_start_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context, expects intdata in cb to have
    //             been set previously
    //  effects: handles execution of the adv start JS callback
    struct zjs_ble_list_item *mycb = CONTAINER_OF(cb, struct zjs_ble_list_item,
                                                  zjs_cb);
    jerry_value_t arg = jerry_create_number(mycb->intdata);
    jerry_value_t rval = jerry_call_function(cb->js_callback, ZJS_UNDEFINED, &arg, 1);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_adv_start_call_function: failed to call function\n");
    }
    jerry_release_value(arg);
    jerry_release_value(rval);
}

const int ZJS_SUCCESS = 0;
const int ZJS_URL_TOO_LONG = 1;
const int ZJS_ALLOC_FAILED = 2;
const int ZJS_URL_SCHEME_ERROR = 3;

static int zjs_encode_url_frame(jerry_value_t url, uint8_t **frame, int *size)
{
    // requires: url is a URL string, frame points to a uint8_t *, url contains
    //             only UTF-8 characters and hence no nil values
    //  effects: allocates a new buffer that will fit an Eddystone URL frame
    //             with a compressed version of the given url; returns it in
    //             *frame and returns the size of the frame in bytes in *size,
    //             and frame is then owned by the caller, to be freed later with
    //             task_free
    //  returns: 0 for success, 1 for URL too long, 2 for out of memory, 3 for
    //             invalid url scheme/syntax (only http:// or https:// allowed)
    jerry_size_t sz = jerry_get_string_size(url);
    char buf[sz + 1];
    int len = jerry_string_to_char_buffer(url, (jerry_char_t *)buf, sz);
    buf[len] = '\0';

    // make sure it starts with http
    int offset = 0;
    if (strncmp(buf, "http", 4))
        return ZJS_URL_SCHEME_ERROR;
    offset += 4;

    int scheme = 0;
    if (buf[offset] == 's') {
        scheme++;
        offset++;
    }

    // make sure scheme http/https is followed by ://
    if (strncmp(buf + offset, "://", 3))
        return ZJS_URL_SCHEME_ERROR;
    offset += 3;

    if (strncmp(buf + offset, "www.", 4)) {
        scheme += 2;
    }
    else {
        offset += 4;
    }

    // FIXME: skipping the compression of .com, .com/, .org, etc for now

    len -= offset;
    if (len > 17)  // max URL length specified by Eddystone spec
        return ZJS_URL_TOO_LONG;

    uint8_t *ptr = task_malloc(len + 5);
    if (!ptr)
        return ZJS_ALLOC_FAILED;

    ptr[0] = 0xaa;  // Eddystone UUID
    ptr[1] = 0xfe;  // Eddystone UUID
    ptr[2] = 0x10;  // Eddystone-URL frame type
    ptr[3] = 0x00;  // calibrated Tx power at 0m
    ptr[4] = scheme; // encoded URL scheme prefix
    strncpy(ptr + 5, buf + offset, len);

    *size = len + 5;
    *frame = ptr;
    return ZJS_SUCCESS;
}

static jerry_value_t zjs_ble_start_advertising(const jerry_value_t function_obj_val,
                                               const jerry_value_t this_val,
                                               const jerry_value_t args_p[],
                                               const jerry_length_t args_cnt)
{
    char name[80];

    if (args_cnt < 2 ||
        !jerry_value_is_string(args_p[0]) ||
        !jerry_value_is_object(args_p[1]) ||
        (args_cnt >= 3 && !jerry_value_is_string(args_p[2]))) {
        PRINT("zjs_ble_adv_start: invalid arguments\n");
        return zjs_error("zjs_ble_adv_start: invalid arguments");
    }

    jerry_value_t array = args_p[1];
    if (!jerry_value_is_array(array)) {
        PRINT("zjs_ble_adv_start: expected array\n");
        return zjs_error("zjs_ble_adv_start: expected array");
    }

    jerry_size_t sz = jerry_get_string_size(args_p[0]);
    int len_name = jerry_string_to_char_buffer(args_p[0],
                                               (jerry_char_t *) name,
                                               sz);
    name[len_name] = '\0';

    struct bt_data sd[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, name, len_name),
    };

    /*
     * Set Advertisement data. Based on the Eddystone specification:
     * https://github.com/google/eddystone/blob/master/protocol-specification.md
     * https://github.com/google/eddystone/tree/master/eddystone-url
     */
    uint8_t *url_frame = NULL;
    int frame_size;
    if (args_cnt >= 3) {
        if (zjs_encode_url_frame(args_p[2], &url_frame, &frame_size)) {
            PRINT("zjs_ble_start_advertising: error encoding url frame, won't be advertised\n");

            // TODO: Make use of error values and turn them into exceptions
        }
    }

    uint32_t arraylen = jerry_get_array_length(array);
    int records = arraylen;
    if (url_frame)
        records += 2;

    if (index + records == 0) {
        PRINT("zjs_ble_adv_start: nothing to advertise\n");
        return zjs_error("zjs_ble_adv_start: nothing to advertise");
    }

    const uint8_t url_adv[] = { 0xaa, 0xfe };

    struct bt_data ad[records];
    int index = 0;
    if (url_frame) {
        ad[0].type = BT_DATA_UUID16_ALL;
        ad[0].data_len = 2;
        ad[0].data = url_adv;

        ad[1].type = BT_DATA_SVC_DATA16;
        ad[1].data_len = frame_size;
        ad[1].data = url_frame;

        index = 2;
    }

    for (int i=0; i<arraylen; i++) {
        jerry_value_t uuid;
        uuid = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_string(uuid)) {
            PRINT("zjs_ble_adv_start: invalid uuid argument type\n");
            return zjs_error("zjs_ble_adv_start: invalid uuid argument type");
        }

        jerry_size_t size = jerry_get_string_size(uuid);
        if (size != 4) {
            PRINT("zjs_ble_adv_start: unexpected uuid string length\n");
            return zjs_error("zjs_ble_adv_start: unexpected uuid string length");
        }

        char ubuf[4];
        uint8_t bytes[2];
        jerry_string_to_char_buffer(uuid, (jerry_char_t *)ubuf, 4);
        if (!zjs_hex_to_byte(ubuf + 2, &bytes[0]) ||
            !zjs_hex_to_byte(ubuf, &bytes[1])) {
            PRINT("zjs_ble_adv_start: invalid character in uuid string\n");
            return zjs_error("zjs_ble_adv_start: invalid character in uuid string");
        }

        ad[index].type = BT_DATA_UUID16_ALL;
        ad[index].data_len = 2;
        ad[index].data = bytes;
        index++;
    }

    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    PRINT("====>AdvertisingStarted..........\n");
    zjs_ble_queue_dispatch("advertisingStart", zjs_ble_adv_start_call_function, err);

    task_free(url_frame);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_ble_stop_advertising(const jerry_value_t function_obj_val,
                                              const jerry_value_t this_val,
                                              const jerry_value_t args_p[],
                                              const jerry_length_t args_cnt)
{
    PRINT("zjs_ble_stop_advertising: stopAdvertising has been called\n");
    return ZJS_UNDEFINED;
}

static bool zjs_ble_parse_characteristic(jerry_value_t chrc_obj,
                                         struct zjs_ble_characteristic *chrc)
{
    char uuid[ZJS_BLE_UUID_LEN];
    if (!zjs_obj_get_string(chrc_obj, "uuid", uuid, ZJS_BLE_UUID_LEN)) {
        PRINT("zjs_ble_parse_characteristic: characteristic uuid doesn't exist\n");
        return false;
    }

    chrc->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_array = zjs_get_property(chrc_obj, "properties");
    if (jerry_value_has_error_flag(v_array)) {
        PRINT("zjs_ble_parse_characteristic: properties doesn't exist\n");
        return false;
    }

    int p_index = 0;
    jerry_value_t v_property;
    v_property = jerry_get_property_by_index(v_array, p_index);
    if (jerry_value_has_error_flag(v_property)) {
        PRINT("zjs_ble_parse_characteristic: failed to access property array\n");
        return false;
    }

    chrc->flags = 0;
    while (jerry_value_is_string(v_property)) {
        char name[20];
        jerry_size_t sz;
        sz = jerry_get_string_size(v_property);
        int len = jerry_string_to_char_buffer(v_property,
                                              (jerry_char_t *) name,
                                              sz);
        name[len] = '\0';

        if (!strcmp(name, "read")) {
            chrc->flags |= BT_GATT_CHRC_READ;
        } else if (!strcmp(name, "write")) {
            chrc->flags |= BT_GATT_CHRC_WRITE;
        } else if (!strcmp(name, "notify")) {
            chrc->flags |= BT_GATT_CHRC_NOTIFY;
        }

        // next property object
        p_index++;
        v_property = jerry_get_property_by_index(v_array, p_index);
        if (jerry_value_has_error_flag(v_property)) {
            PRINT("zjs_ble_parse_characteristic: failed to access property array\n");
            return false;
        }
    }

    jerry_value_t v_func;
    v_func = zjs_get_property(chrc_obj, "onReadRequest");
    if (jerry_value_is_function(v_func)) {
        chrc->read_cb.zjs_cb.js_callback = jerry_acquire_value(v_func);
    }

    v_func = zjs_get_property(chrc_obj, "onWriteRequest");
    if (jerry_value_is_function(v_func)) {
        chrc->write_cb.zjs_cb.js_callback = jerry_acquire_value(v_func);
    }

    v_func = zjs_get_property(chrc_obj, "onSubscribe");
    if (jerry_value_is_function(v_func)) {
        chrc->subscribe_cb.zjs_cb.js_callback = jerry_acquire_value(v_func);
        // TODO: we need to monitor onSubscribe events from BLE driver eventually
        zjs_ble_subscribe_call_function(&chrc->subscribe_cb.zjs_cb);
    }

    v_func = zjs_get_property(chrc_obj, "onUnsubscribe");
    if (jerry_value_is_function(v_func)) {
        chrc->unsubscribe_cb.zjs_cb.js_callback = jerry_acquire_value(v_func);
    }

    v_func = zjs_get_property(chrc_obj, "onNotify");
    if (jerry_value_is_function(v_func)) {
        chrc->notify_cb.zjs_cb.js_callback = jerry_acquire_value(v_func);
    }

    return true;
}

static bool zjs_ble_parse_service(jerry_value_t service_obj,
                                  struct zjs_ble_service *service)
{
    char uuid[ZJS_BLE_UUID_LEN];
    if (!zjs_obj_get_string(service_obj, "uuid", uuid, ZJS_BLE_UUID_LEN)) {
        PRINT("zjs_ble_parse_service: service uuid doesn't exist\n");
        return false;
    }
    service->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_array = zjs_get_property(service_obj, "characteristics");
    if (jerry_value_has_error_flag(v_array)) {
        PRINT("zjs_ble_parse_service: characteristics doesn't exist\n");
        return false;
    }

    if (!jerry_value_is_object(v_array)) {
        PRINT("zjs_ble_parse_service: characteristics array is undefined or null\n");
        return false;
    }

    int chrc_index = 0;
    jerry_value_t v_characteristic;
    v_characteristic = jerry_get_property_by_index(v_array, chrc_index);
    if (jerry_value_has_error_flag(v_characteristic)) {
        PRINT("zjs_ble_parse_service: failed to access characteristic array\n");
        return false;
    }

    struct zjs_ble_characteristic *previous = NULL;
    while (jerry_value_is_object(v_characteristic)) {
        struct zjs_ble_characteristic *chrc = task_malloc(sizeof(struct zjs_ble_characteristic));
        if (!chrc) {
            PRINT("zjs_ble_parse_service: out of memory allocating struct zjs_ble_characteristic\n");
            return false;
        } else {
            memset(chrc, 0, sizeof(struct zjs_ble_characteristic));
        }

        chrc->chrc_obj = jerry_acquire_value(v_characteristic);
        jerry_set_object_native_handle(chrc->chrc_obj, (uintptr_t)chrc, NULL);

        if (!zjs_ble_parse_characteristic(chrc->chrc_obj, chrc)) {
            PRINT("failed to parse temp characteristic\n");
            return false;
        }

        // append to the list
        if (!service->characteristics) {
            service->characteristics = chrc;
            previous = chrc;
        }
        else {
           previous->next = chrc;
        }

        // next characterstic
        chrc_index++;
        v_characteristic = jerry_get_property_by_index(v_array, chrc_index);
        if (jerry_value_has_error_flag(v_characteristic)) {
            PRINT("zjs_ble_parse_service: failed to access characteristic array\n");
            return false;
        }
    }

    return true;
}

/*
 * WebBT Service Declaration
 *
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(BT_UUID_WEBBT),

    BT_GATT_CHARACTERISTIC(BT_UUID_TEMP,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
    BT_GATT_DESCRIPTOR(BT_UUID_TEMP, BT_GATT_PERM_READ,
        read_callback, NULL, NULL),
    BT_GATT_CUD(SENSOR_1_NAME, BT_GATT_PERM_READ),
    BT_GATT_CCC(blvl_ccc_cfg, blvl_ccc_cfg_changed),

    BT_GATT_CHARACTERISTIC(BT_UUID_RGB,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_RGB, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_callback, write_callback, NULL),
    BT_GATT_CUD(SENSOR_2_NAME, BT_GATT_PERM_READ),
};
*/

static bool zjs_ble_register_service(struct zjs_ble_service *service)
{
    if (!service) {
        PRINT("zjs_ble_register_service: invalid ble_service\n");
        return false;
    }

    int num_of_entries = 1; // 1 attribute for service uuid
    struct zjs_ble_characteristic *ch = service->characteristics;
    while (ch != NULL) {
        num_of_entries += 4;  // 4 attributes per characteristic
        ch = ch->next;
    }

   struct bt_gatt_attr* bt_attrs = task_malloc(sizeof(struct bt_gatt_attr) * num_of_entries);
    if (!bt_attrs) {
        PRINT("zjs_ble_register_service: out of memory allocating struct bt_gatt_attr\n");
        return false;
    } else {
        memset(bt_attrs, 0, sizeof(struct bt_gatt_attr) * num_of_entries);
    }

    // SERVICE
    int entry_index = 0;
    bt_attrs[entry_index].uuid = BT_UUID_GATT_PRIMARY;
    bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
    bt_attrs[entry_index].read = bt_gatt_attr_read_service;
    bt_attrs[entry_index].user_data = service->uuid;
    entry_index++;

    ch = service->characteristics;
    while (ch != NULL) {
        // CHARACTERISTIC
        struct bt_gatt_chrc *chrc_user_data = task_malloc(sizeof(struct bt_gatt_chrc));
        if (!chrc_user_data) {
            PRINT("error: out of memory allocating struct bt_gatt_chrc\n");
            return false;
        } else {
            memset(chrc_user_data, 0, sizeof(struct bt_gatt_chrc));
        }

        chrc_user_data->uuid = ch->uuid;
        chrc_user_data->properties = ch->flags;
        bt_attrs[entry_index].uuid = BT_UUID_GATT_CHRC;
        bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
        bt_attrs[entry_index].read = bt_gatt_attr_read_chrc;
        bt_attrs[entry_index].user_data = chrc_user_data;

        // TODO: handle multiple descriptors
        // DESCRIPTOR
        entry_index++;
        bt_attrs[entry_index].uuid = ch->uuid;
        if (ch->read_cb.zjs_cb.js_callback) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_READ;
        }
        if (ch->write_cb.zjs_cb.js_callback) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_WRITE;
        }
        bt_attrs[entry_index].read = zjs_ble_read_attr_callback;
        bt_attrs[entry_index].write = zjs_ble_write_attr_callback;
        bt_attrs[entry_index].user_data = ch;

        // hold references to the GATT attr for sending notification
        ch->chrc_attr = &bt_attrs[entry_index];

        if (!bt_uuid_cmp(ch->uuid, BT_UUID_TEMP)) {
            // CUD
            // FIXME: only temperature sets it for now
            entry_index++;
            bt_attrs[entry_index].uuid = BT_UUID_GATT_CUD;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
            bt_attrs[entry_index].read = bt_gatt_attr_read_cud;
            bt_attrs[entry_index].user_data = SENSOR_1_NAME;

            // BT_GATT_CCC
            entry_index++;
            // FIXME: for notification?
            struct _bt_gatt_ccc *ccc_user_data = task_malloc(sizeof(struct _bt_gatt_ccc));
            if (!ccc_user_data) {
                PRINT("error: out of memory allocating struct bt_gatt_ccc\n");
                return false;
            } else {
                memset(ccc_user_data, 0, sizeof(struct _bt_gatt_ccc));
            }

            ccc_user_data->cfg = zjs_ble_blvl_ccc_cfg;
            ccc_user_data->cfg_len = ARRAY_SIZE(zjs_ble_blvl_ccc_cfg);
            ccc_user_data->cfg_changed = zjs_ble_blvl_ccc_cfg_changed;
            bt_attrs[entry_index].uuid = BT_UUID_GATT_CCC;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
            bt_attrs[entry_index].read = bt_gatt_attr_read_ccc;
            bt_attrs[entry_index].write = bt_gatt_attr_write_ccc;
            bt_attrs[entry_index].user_data = ccc_user_data;
        } else if (!bt_uuid_cmp(ch->uuid, BT_UUID_RGB)) {
            entry_index++;
            bt_attrs[entry_index].uuid = BT_UUID_GATT_CUD;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
            bt_attrs[entry_index].read = bt_gatt_attr_read_cud;
            bt_attrs[entry_index].user_data = SENSOR_2_NAME;

            entry_index++;    //placeholder
            bt_attrs[entry_index].uuid = BT_UUID_GATT_CCC;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
            bt_attrs[entry_index].read = bt_gatt_attr_read_ccc;
            bt_attrs[entry_index].write = bt_gatt_attr_write_ccc;
        }

        entry_index++;
        ch = ch->next;
    }

    PRINT("register service %d entries\n", entry_index);
    bt_gatt_register(bt_attrs, entry_index);
    return true;
}

static jerry_value_t zjs_ble_set_services(const jerry_value_t function_obj_val,
                                          const jerry_value_t this_val,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_cnt)
{
    PRINT("setServices has been called\n");

    if (args_cnt < 1 || !jerry_value_is_object(args_p[0]))
    {
        PRINT("zjs_ble_set_services: invalid arguments\n");
        return false;
    }

    if (args_cnt == 2 && !jerry_value_is_object(args_p[1]))
    {
        PRINT("zjs_ble_set_services: invalid arguments\n");
        return false;
    }

    // FIXME: currently hard-coded to work with demo
    // which has only 1 primary service and 2 characterstics
    // add support for multiple services
    jerry_value_t v_service;
    v_service = jerry_get_property_by_index(args_p[0], 0);
    if (jerry_value_has_error_flag(v_service)) {
        PRINT("zjs_ble_set_services: services array is empty\n");
        return zjs_error("services array is empty");
    }

    if (zjs_ble_service.characteristics) {
        zjs_ble_free_characteristics(zjs_ble_service.characteristics);
    }

    zjs_ble_service.service_obj = jerry_acquire_value(v_service);
    jerry_set_object_native_handle(zjs_ble_service.service_obj,
                                   (uintptr_t)&zjs_ble_service, NULL);

    if (!zjs_ble_parse_service(zjs_ble_service.service_obj, &zjs_ble_service)) {
        PRINT("zjs_ble_set_services: failed to validate service object\n");
        return zjs_error("zjs_ble_set_services: failed to validate service object");
    }

    if (zjs_ble_register_service(&zjs_ble_service)) {
        return ZJS_UNDEFINED;
    } else {
        PRINT("zjs_ble_set_services: failed to register service\n");
        return zjs_error("zjs_ble_set_services: failed to register service");
    }
}

// Constructor
static jerry_value_t zjs_ble_primary_service(const jerry_value_t function_obj_val,
                                             const jerry_value_t this_val,
                                             const jerry_value_t args_p[],
                                             const jerry_length_t args_cnt)
{
    PRINT("new PrimaryService has been called\n");

    if (args_cnt < 1 || !jerry_value_is_object(args_p[0]))
    {
        PRINT("zjs_ble_primary_service: invalid arguments\n");
        return zjs_error("zjs_ble_primary_service: invalid arguments");
    }

    return jerry_acquire_value(args_p[0]);
}

// Constructor
static jerry_value_t zjs_ble_characteristic(const jerry_value_t function_obj_val,
                                            const jerry_value_t this_val,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_cnt)
{
    PRINT("new Characterstic has been called\n");

    if (args_cnt < 1 || !jerry_value_is_object(args_p[0]))
    {
        PRINT("zjs_ble_characterstic: invalid arguments\n");
        return zjs_error("invalid arguments");
    }

    jerry_value_t obj = jerry_acquire_value(args_p[0]);
    jerry_value_t val;

    // error codes
    val = jerry_create_number(ZJS_BLE_RESULT_SUCCESS);
    zjs_set_property(obj, "RESULT_SUCCESS", val);
    jerry_release_value(val);

    val = jerry_create_number(ZJS_BLE_RESULT_INVALID_OFFSET);
    zjs_set_property(obj, "RESULT_INVALID_OFFSET", val);
    jerry_release_value(val);

    val = jerry_create_number(ZJS_BLE_RESULT_ATTR_NOT_LONG);
    zjs_set_property(obj, "RESULT_ATTR_NOT_LONG", val);
    jerry_release_value(val);

    val = jerry_create_number(ZJS_BLE_RESULT_INVALID_ATTRIBUTE_LENGTH);
    zjs_set_property(obj, "RESULT_INVALID_ATTRIBUTE_LENGTH", val);
    jerry_release_value(val);

    val = jerry_create_number(ZJS_BLE_RESULT_UNLIKELY_ERROR);
    zjs_set_property(obj, "RESULT_UNLIKELY_ERROR", val);
    jerry_release_value(val);

    return args_p[0];
}

jerry_value_t zjs_ble_init()
{
     nano_sem_init(&zjs_ble_nano_sem);

    // create global BLE object
    jerry_value_t ble_obj = jerry_create_object();
    zjs_obj_add_function(ble_obj, zjs_ble_on, "on");
    zjs_obj_add_function(ble_obj, zjs_ble_start_advertising, "startAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_stop_advertising, "stopAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_set_services, "setServices");

    // PrimaryService and Characteristic constructors
    zjs_obj_add_function(ble_obj, zjs_ble_primary_service, "PrimaryService");
    zjs_obj_add_function(ble_obj, zjs_ble_characteristic, "Characteristic");
    return ble_obj;
}
#endif
