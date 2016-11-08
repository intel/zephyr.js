// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_BLE
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
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_util.h"

#define ZJS_BLE_UUID_LEN                            36

#define ZJS_BLE_RESULT_SUCCESS                      0x00
#define ZJS_BLE_RESULT_INVALID_OFFSET               BT_ATT_ERR_INVALID_OFFSET
#define ZJS_BLE_RESULT_ATTR_NOT_LONG                BT_ATT_ERR_ATTRIBUTE_NOT_LONG
#define ZJS_BLE_RESULT_INVALID_ATTRIBUTE_LENGTH     BT_ATT_ERR_INVALID_ATTRIBUTE_LEN
#define ZJS_BLE_RESULT_UNLIKELY_ERROR               BT_ATT_ERR_UNLIKELY

#define ZJS_BLE_TIMEOUT_TICKS                       500

struct nano_sem zjs_ble_nano_sem;

typedef struct ble_handle {
    int32_t id;
    jerry_value_t js_callback;
    const void *buffer;
    uint16_t buffer_size;
    uint16_t offset;
    uint32_t error_code;
} ble_handle_t;

typedef struct ble_notify_handle {
    int32_t id;
    jerry_value_t js_callback;
} ble_notify_handle_t;

typedef struct ble_event_handle {
    int32_t id;
    jerry_value_t arg;
} ble_event_handle_t;

typedef struct zjs_ble_characteristic {
    int flags;
    jerry_value_t chrc_obj;
    struct bt_uuid *uuid;
    struct bt_gatt_attr *chrc_attr;
    jerry_value_t cud_value;
    ble_handle_t read_cb;
    ble_handle_t write_cb;
    ble_notify_handle_t subscribe_cb;
    ble_notify_handle_t unsubscribe_cb;
    ble_notify_handle_t notify_cb;
    struct zjs_ble_characteristic *next;
} ble_characteristic_t;

typedef struct zjs_ble_service {
    jerry_value_t service_obj;
    struct bt_uuid *uuid;
    ble_characteristic_t *characteristics;
    struct zjs_ble_service *next;
} ble_service_t;

typedef struct zjs_ble_connection {
    jerry_value_t ble_obj;
    struct bt_conn *default_conn;
    struct bt_gatt_ccc_cfg blvl_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED];
    uint8_t simulate_blvl;
    ble_service_t *services;
    ble_event_handle_t ready_cb;
    ble_event_handle_t connected_cb;
    ble_event_handle_t disconnected_cb;
} ble_connection_t;

// global connection object
static struct zjs_ble_connection *ble_conn = &(struct zjs_ble_connection) {
    .default_conn = NULL,
    .blvl_ccc_cfg = {},
    .simulate_blvl = 0,
    .services = NULL,
};

static struct bt_uuid *gatt_primary_service_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_PRIMARY_VAL);
static struct bt_uuid *gatt_characteristic_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_CHRC_VAL);
static struct bt_uuid *gatt_cud_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_CUD_VAL);
static struct bt_uuid *gatt_ccc_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_CCC_VAL);

struct bt_uuid* zjs_ble_new_uuid_16(uint16_t value) {
    struct bt_uuid_16 *uuid = zjs_malloc(sizeof(struct bt_uuid_16));
    if (!uuid) {
        PRINT("zjs_ble_new_uuid_16: out of memory allocating struct bt_uuid_16\n");
        return NULL;
    }

    memset(uuid, 0, sizeof(struct bt_uuid_16));

    uuid->uuid.type = BT_UUID_TYPE_16;
    uuid->val = value;
    return (struct bt_uuid *) uuid;
}

static void zjs_ble_free_characteristics(ble_characteristic_t *chrc)
{
    ble_characteristic_t *tmp;
    while (chrc != NULL) {
        tmp = chrc;
        chrc = chrc->next;

        jerry_release_value(tmp->chrc_obj);

        if (tmp->uuid)
            zjs_free(tmp->uuid);
        if (tmp->read_cb.id != -1) {
            zjs_remove_callback(tmp->read_cb.id);
            jerry_release_value(tmp->read_cb.js_callback);
        }
        if (tmp->write_cb.id != -1) {
            zjs_remove_callback(tmp->write_cb.id);
            jerry_release_value(tmp->write_cb.js_callback);
        }
        if (tmp->subscribe_cb.id != -1) {
            zjs_remove_callback(tmp->subscribe_cb.id);
            jerry_release_value(tmp->subscribe_cb.js_callback);
        }
        if (tmp->unsubscribe_cb.id != -1) {
            zjs_remove_callback(tmp->unsubscribe_cb.id);
            jerry_release_value(tmp->unsubscribe_cb.js_callback);
        }
        if (tmp->notify_cb.id != -1) {
            zjs_remove_callback(tmp->notify_cb.id);
            jerry_release_value(tmp->notify_cb.js_callback);
        }

        zjs_free(tmp);
    }
}

static void zjs_ble_free_services(ble_service_t *service)
{
    ble_service_t *tmp;
    while (service != NULL) {
        tmp = service;
        service = service->next;

        jerry_release_value(tmp->service_obj);

        if (tmp->uuid)
            zjs_free(tmp->uuid);
        if (tmp->characteristics)
            zjs_ble_free_characteristics(tmp->characteristics);

        zjs_free(tmp);
    }
}

static jerry_value_t zjs_ble_read_callback_function(const jerry_value_t function_obj,
                                                    const jerry_value_t this,
                                                    const jerry_value_t argv[],
                                                    const jerry_length_t argc)
{
    if (argc != 2 ||
        !jerry_value_is_number(argv[0]) ||
        !jerry_value_is_object(argv[1])) {
        nano_task_sem_give(&zjs_ble_nano_sem);
        return zjs_error("zjs_ble_read_attr_call_function_return: invalid arguments");
    }

    uintptr_t ptr;
    if (jerry_get_object_native_handle(function_obj, &ptr)) {
        // store the return value in the read_cb struct
        struct zjs_ble_characteristic *chrc = (struct zjs_ble_characteristic*)ptr;
        chrc->read_cb.error_code = (uint32_t)jerry_get_number_value(argv[0]);

        zjs_buffer_t *buf = zjs_buffer_find(argv[1]);
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

static void zjs_ble_read_c_callback(void *handle, void* argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_handle_t *cb = &chrc->read_cb;

    jerry_value_t rval;
    jerry_value_t args[2];
    jerry_value_t func_obj;

    args[0] = jerry_create_number(cb->offset);
    func_obj = jerry_create_external_function(zjs_ble_read_callback_function);
    jerry_set_object_native_handle(func_obj, (uintptr_t)handle, NULL);
    args[1] = func_obj;

    rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_read_c_callback: failed to call onReadRequest function\n");
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

    ble_characteristic_t *chrc = attr->user_data;

    if (!chrc) {
        PRINT("zjs_ble_read_attr_callback: characteristic not found\n");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
    }

    if (chrc->read_cb.id != -1) {
        // This is from the FIBER context, so we queue up the callback
        // to invoke js from task context
        chrc->read_cb.offset = offset;
        chrc->read_cb.buffer = NULL;
        chrc->read_cb.buffer_size = 0;
        chrc->read_cb.error_code = BT_ATT_ERR_NOT_SUPPORTED;
        zjs_signal_callback(chrc->read_cb.id, NULL, 0);

        // block until result is ready
        if (!nano_fiber_sem_take(&zjs_ble_nano_sem, ZJS_BLE_TIMEOUT_TICKS)) {
            PRINT("zjs_ble_read_attr_callback: JS callback timed out\n");
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }

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
            PRINT("zjs_ble_read_attr_callback: on read attr error %lu\n",
                  chrc->read_cb.error_code);
            return BT_GATT_ERR(chrc->read_cb.error_code);
        }
    }

    PRINT("zjs_ble_read_attr_callback: js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static jerry_value_t zjs_ble_write_callback_function(const jerry_value_t function_obj,
                                                     const jerry_value_t this,
                                                     const jerry_value_t argv[],
                                                     const jerry_length_t argc)
{
    if (argc != 1 ||
        !jerry_value_is_number(argv[0])) {
        nano_task_sem_give(&zjs_ble_nano_sem);
        return zjs_error("zjs_ble_write_attr_call_function_return: invalid arguments");
    }

    uintptr_t ptr;
    if (jerry_get_object_native_handle(function_obj, &ptr)) {
        // store the return value in the write_cb struct
        struct zjs_ble_characteristic *chrc = (struct zjs_ble_characteristic*)ptr;
        chrc->write_cb.error_code = (uint32_t)jerry_get_number_value(argv[0]);
    }

    // unblock fiber
    nano_task_sem_give(&zjs_ble_nano_sem);
    return ZJS_UNDEFINED;
}

static void zjs_ble_write_c_callback(void *handle, void* argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_handle_t *cb = &chrc->write_cb;

    jerry_value_t rval;
    jerry_value_t args[4];
    jerry_value_t func_obj;

    args[0] = jerry_create_null();
    if (cb->buffer && cb->buffer_size > 0) {
        jerry_value_t buf_obj = zjs_buffer_create(cb->buffer_size);

        if (buf_obj) {
           zjs_buffer_t *buf = zjs_buffer_find(buf_obj);

           if (buf &&
               buf->buffer &&
               buf->bufsize == cb->buffer_size) {
               memcpy(buf->buffer, cb->buffer, cb->buffer_size);
           }

           jerry_release_value(args[0]);
           args[0] = buf_obj;
        }
    }
    args[1] = jerry_create_number(cb->offset);
    // TODO: support withoutResponse flag
    args[2] = jerry_create_boolean(false);
    func_obj = jerry_create_external_function(zjs_ble_write_callback_function);
    jerry_set_object_native_handle(func_obj, (uintptr_t)handle, NULL);
    args[3] = func_obj;

    rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 4);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_write_c_callback: failed to call onWriteRequest function\n");
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
    ble_characteristic_t *chrc = attr->user_data;

    if (!chrc) {
        PRINT("zjs_ble_write_attr_callback: characteristic not found\n");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
    }

    if (chrc->write_cb.id != -1) {
        // This is from the FIBER context, so we queue up the callback
        // to invoke js from task context
        chrc->write_cb.offset = offset;
        chrc->write_cb.buffer = (len > 0) ? buf : NULL;
        chrc->write_cb.buffer_size = len;
        chrc->write_cb.error_code = BT_ATT_ERR_NOT_SUPPORTED;
        zjs_signal_callback(chrc->write_cb.id, NULL, 0);

        // block until result is ready
        if (!nano_fiber_sem_take(&zjs_ble_nano_sem, ZJS_BLE_TIMEOUT_TICKS)) {
            PRINT("zjs_ble_write_attr_callback: JS callback timed out\n");
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }

        if (chrc->write_cb.error_code == ZJS_BLE_RESULT_SUCCESS) {
            return len;
        } else {
            return BT_GATT_ERR(chrc->write_cb.error_code);
        }
    }

    PRINT("zjs_ble_write_attr_callback: js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static jerry_value_t zjs_ble_update_value_callback_function(const jerry_value_t function_obj,
                                                            const jerry_value_t this,
                                                            const jerry_value_t argv[],
                                                            const jerry_length_t argc)
{
    if (argc != 1 ||
        !jerry_value_is_object(argv[0])) {
        return zjs_error("zjs_ble_update_value_call_function: invalid arguments");
    }

    // expects a Buffer object
    zjs_buffer_t *buf = zjs_buffer_find(argv[0]);

    if (buf) {
        if (ble_conn->default_conn) {
            uintptr_t ptr;
            if (jerry_get_object_native_handle(this, &ptr)) {
               ble_characteristic_t *chrc = (ble_characteristic_t *)ptr;
               if (chrc->chrc_attr) {
                   bt_gatt_notify(ble_conn->default_conn, chrc->chrc_attr,
                                  buf->buffer, buf->bufsize);
               }
            }
        }

        return ZJS_UNDEFINED;
    }

    return zjs_error("updateValueCallback: buffer not found or empty");
}

static void zjs_ble_subscribe_c_callback(void *handle, void* argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->subscribe_cb;

    jerry_value_t rval;
    jerry_value_t args[2];

    args[0] = jerry_create_number(20); // max payload size
    args[1] = jerry_create_external_function(zjs_ble_update_value_callback_function);
    rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_subscribe_c_callback: failed to call onSubscribe function\n");
    }

    jerry_release_value(args[0]);
    jerry_release_value(args[1]);
    jerry_release_value(rval);
}

static void zjs_ble_unsubscribe_c_callback(void *handle, void* argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->unsubscribe_cb;

    jerry_value_t rval;

    rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, NULL, 0);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_unsubscribe_c_callback: failed to call onUnsubscribe function\n");
    }

    jerry_release_value(rval);
}

static void zjs_ble_notify_c_callback(void *handle, void* argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->notify_cb;

    jerry_value_t rval;

    rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, NULL, 0);
    if (jerry_value_has_error_flag(rval)) {
        PRINT("zjs_ble_notify_c_callback: failed to call onNotify function\n");
    }

    jerry_release_value(rval);
}

// Port this to javascript
static void zjs_ble_blvl_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ble_conn->simulate_blvl = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void zjs_ble_connected_c_callback(void *handle, void* argv)
{
    // FIXME: get real bluetooth address
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"AB:CD:DF:AB:CD:EF");
    zjs_trigger_event(ble_conn->ble_obj, "accept", &arg, 1, NULL, NULL);
    DBG_PRINT("BLE event: accept\n");
}

static void zjs_ble_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        DBG_PRINT("zjs_ble_connected: Connection failed (err %u)\n", err);
    } else {
        DBG_PRINT("========== connected ==========\n");
        ble_conn->default_conn = bt_conn_ref(conn);
        zjs_signal_callback(ble_conn->connected_cb.id, NULL, 0);
    }
}

static void zjs_ble_disconnected_c_callback(void *handle, void* argv)
{
    // FIXME: get real bluetooth address
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"AB:CD:DF:AB:CD:EF");
    zjs_trigger_event(ble_conn->ble_obj, "disconnect", &arg, 1, NULL, NULL);
    DBG_PRINT("BLE event: disconnect\n");
}

static void zjs_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
    DBG_PRINT("========== Disconnected (reason %u) ==========\n", reason);
    if (ble_conn->default_conn) {
        bt_conn_unref(ble_conn->default_conn);
        ble_conn->default_conn = NULL;
        zjs_signal_callback(ble_conn->disconnected_cb.id, NULL, 0);
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

static void zjs_ble_ready_c_callback(void *handle, void* argv)
{
    jerry_value_t arg = jerry_create_string((jerry_char_t *)"poweredOn");
    zjs_trigger_event(ble_conn->ble_obj, "stateChange", &arg, 1, NULL, NULL);
    DBG_PRINT("BLE event: stateChange - poweredOn");
}

static void zjs_ble_bt_ready(int err)
{
    DBG_PRINT("bt_ready() is called [err %d]\n", err);
    zjs_signal_callback(ble_conn->ready_cb.id, NULL, 0);
}

void zjs_ble_enable() {
    DBG_PRINT("Enabling the bluetooth, wait for bt_ready()...\n");
    bt_enable(zjs_ble_bt_ready);
    // setup connection callbacks
    bt_conn_cb_register(&zjs_ble_conn_callbacks);
    bt_conn_auth_cb_register(&zjs_ble_auth_cb_display);
}

static jerry_value_t zjs_ble_disconnect(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    if (ble_conn->default_conn) {
        int error = bt_conn_disconnect(ble_conn->default_conn,
                                       BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (error) {
            return zjs_error("zjs_ble_disconnect: disconnect failed");
        }
    }

    return ZJS_UNDEFINED;
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
    //             zjs_free
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

    uint8_t *ptr = zjs_malloc(len + 5);
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

static jerry_value_t zjs_ble_start_advertising(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    // arg 0 should be the device name to advertise, e.g. "Arduino101"
    // arg 1 should be an array of UUIDs (short, 4 hex chars)
    // arg 2 should be a short URL (typically registered with Google, I think)
    char name[80];

    if (argc < 2 ||
        !jerry_value_is_string(argv[0]) ||
        !jerry_value_is_object(argv[1]) ||
        (argc >= 3 && !jerry_value_is_string(argv[2]))) {
        return zjs_error("zjs_ble_adv_start: invalid arguments");
    }

    jerry_value_t array = argv[1];
    if (!jerry_value_is_array(array)) {
        return zjs_error("zjs_ble_adv_start: expected array");
    }

    jerry_size_t sz = jerry_get_string_size(argv[0]);
    int len_name = jerry_string_to_char_buffer(argv[0],
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
    if (argc >= 3) {
        if (zjs_encode_url_frame(argv[2], &url_frame, &frame_size)) {
            PRINT("zjs_ble_start_advertising: error encoding url frame, won't be advertised\n");

            // TODO: Make use of error values and turn them into exceptions
        }
    }

    uint32_t arraylen = jerry_get_array_length(array);
    int records = arraylen;
    if (url_frame)
        records += 2;

    if (records == 0) {
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
            return zjs_error("zjs_ble_adv_start: invalid uuid argument type");
        }

        jerry_size_t size = jerry_get_string_size(uuid);
        if (size != 4) {
            return zjs_error("zjs_ble_adv_start: unexpected uuid string length");
        }

        char ubuf[4];
        uint8_t bytes[2];
        jerry_string_to_char_buffer(uuid, (jerry_char_t *)ubuf, 4);
        if (!zjs_hex_to_byte(ubuf + 2, &bytes[0]) ||
            !zjs_hex_to_byte(ubuf, &bytes[1])) {
            return zjs_error("zjs_ble_adv_start: invalid character in uuid string");
        }

        ad[index].type = BT_DATA_UUID16_ALL;
        ad[index].data_len = 2;
        ad[index].data = bytes;
        index++;
    }

    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    jerry_value_t error = err ? zjs_error("advertising failed") :
                                jerry_create_null();

    zjs_trigger_event(ble_conn->ble_obj, "advertisingStart", &error, 1, NULL, NULL);
    DBG_PRINT("BLE event: adveristingStart\n");

    zjs_free(url_frame);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_ble_stop_advertising(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    DBG_PRINT("zjs_ble_stop_advertising: stopAdvertising has been called\n");
    return ZJS_UNDEFINED;
}

static bool zjs_ble_parse_characteristic(ble_characteristic_t *chrc)
{
    char uuid[ZJS_BLE_UUID_LEN];
    if (!chrc || !chrc->chrc_obj)
        return false;

    jerry_value_t chrc_obj = chrc->chrc_obj;
    if (!zjs_obj_get_string(chrc_obj, "uuid", uuid, ZJS_BLE_UUID_LEN)) {
        PRINT("zjs_ble_parse_characteristic: characteristic uuid doesn't exist\n");
        return false;
    }

    chrc->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_array = zjs_get_property(chrc_obj, "properties");
    if (!jerry_value_is_array(v_array)) {
        PRINT("zjs_ble_parse_characteristic: properties is empty or not array\n");
        return false;
    }

    for (int i=0; i<jerry_get_array_length(v_array); i++) {
        jerry_value_t v_property = jerry_get_property_by_index(v_array, i);

        if (!jerry_value_is_string(v_property)) {
            PRINT("zjs_ble_parse_characteristic: property is not string\n");
            return false;
        }

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
    }
    jerry_release_value(v_array);

    v_array = zjs_get_property(chrc_obj, "descriptors");
    if (!jerry_value_is_undefined(v_array) &&
        !jerry_value_is_null(v_array) &&
        !jerry_value_is_array(v_array)) {
        PRINT("zjs_ble_parse_characteristic: descriptors is not array\n");
        return false;
    }

    for (int i=0; i<jerry_get_array_length(v_array); i++) {
        jerry_value_t v_desc = jerry_get_property_by_index(v_array, i);

        if (!jerry_value_is_object(v_desc)) {
            PRINT("zjs_ble_parse_characteristic: not valid descriptor object\n");
            return false;
        }

        char desc_uuid[ZJS_BLE_UUID_LEN];
        if (!zjs_obj_get_string(v_desc, "uuid", desc_uuid, ZJS_BLE_UUID_LEN)) {
            PRINT("zjs_ble_parse_service: descriptor uuid doesn't exist\n");
            return false;
        }

        if (strtoul(desc_uuid, NULL, 16) == BT_UUID_GATT_CUD_VAL) {
            // Support CUD only, ignore all other type of descriptors
            jerry_value_t v_value = zjs_get_property(v_desc, "value");
            if (jerry_value_is_string(v_value)) {
                chrc->cud_value = jerry_acquire_value(v_value);
            }
        }
    }
    jerry_release_value(v_array);

    jerry_value_t v_func;
    v_func = zjs_get_property(chrc_obj, "onReadRequest");
    if (jerry_value_is_function(v_func)) {
        chrc->read_cb.js_callback = jerry_acquire_value(v_func);
        chrc->read_cb.id = zjs_add_c_callback(chrc, zjs_ble_read_c_callback);
    } else {
        chrc->read_cb.id = -1;
    }

    v_func = zjs_get_property(chrc_obj, "onWriteRequest");
    if (jerry_value_is_function(v_func)) {
        chrc->write_cb.js_callback = jerry_acquire_value(v_func);
        chrc->write_cb.id = zjs_add_c_callback(chrc, zjs_ble_write_c_callback);
    } else {
        chrc->write_cb.id = -1;
    }

    v_func = zjs_get_property(chrc_obj, "onSubscribe");
    if (jerry_value_is_function(v_func)) {
        chrc->subscribe_cb.js_callback = jerry_acquire_value(v_func);
        chrc->subscribe_cb.id = zjs_add_c_callback(chrc, zjs_ble_subscribe_c_callback);
        // TODO: we need to monitor onSubscribe events from BLE driver eventually
        zjs_signal_callback(chrc->subscribe_cb.id, NULL, 0);
    } else {
        chrc->subscribe_cb.id = -1;
    }

    v_func = zjs_get_property(chrc_obj, "onUnsubscribe");
    if (jerry_value_is_function(v_func)) {
        chrc->unsubscribe_cb.js_callback = jerry_acquire_value(v_func);
        chrc->unsubscribe_cb.id = zjs_add_c_callback(chrc, zjs_ble_unsubscribe_c_callback);
    } else {
        chrc->unsubscribe_cb.id = -1;
    }

    v_func = zjs_get_property(chrc_obj, "onNotify");
    if (jerry_value_is_function(v_func)) {
        chrc->notify_cb.js_callback = jerry_acquire_value(v_func);
        chrc->notify_cb.id = zjs_add_c_callback(chrc, zjs_ble_notify_c_callback);
    } else {
        chrc->notify_cb.id = -1;
    }

    return true;
}

static bool zjs_ble_parse_service(ble_service_t *service)
{
    char uuid[ZJS_BLE_UUID_LEN];
    if (!service || !service->service_obj)
        return false;

    jerry_value_t service_obj = service->service_obj;
    if (!zjs_obj_get_string(service_obj, "uuid", uuid, ZJS_BLE_UUID_LEN)) {
        PRINT("zjs_ble_parse_service: service uuid doesn't exist\n");
        return false;
    }
    service->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_array = zjs_get_property(service_obj, "characteristics");
    if (!jerry_value_is_array(v_array)) {
        PRINT("zjs_ble_parse_service: characteristics is empty or not array\n");
        return false;
    }

    ble_characteristic_t *previous = NULL;
    for (int i=0; i<jerry_get_array_length(v_array); i++) {
        jerry_value_t v_chrc = jerry_get_property_by_index(v_array, i);

        if (!jerry_value_is_object(v_chrc)) {
            PRINT("zjs_ble_parse_characteristic: characteristic is not object\n");
            return false;
        }

        ble_characteristic_t *chrc = zjs_malloc(sizeof(ble_characteristic_t));
        if (!chrc) {
            PRINT("zjs_ble_parse_service: out of memory allocating ble_characteristic_t\n");
            return false;
        }

        memset(chrc, 0, sizeof(ble_characteristic_t));
        chrc->read_cb.id = chrc->write_cb.id
                         = chrc->subscribe_cb.id
                         = chrc->unsubscribe_cb.id
                         = chrc->notify_cb.id
                         = -1;

        chrc->chrc_obj = jerry_acquire_value(v_chrc);
        jerry_set_object_native_handle(chrc->chrc_obj, (uintptr_t)chrc, NULL);

        if (!zjs_ble_parse_characteristic(chrc)) {
            DBG_PRINT("failed to parse temp characteristic\n");
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
    }

    return true;
}

static bool zjs_ble_register_service(ble_service_t *service)
{
    if (!service) {
        PRINT("zjs_ble_register_service: invalid ble_service\n");
        return false;
    }

    // calculate the number of GATT attributes to allocate
    int entry_index = 0;
    int num_of_entries = 1;   // 1 attribute for service uuid
    ble_characteristic_t *ch = service->characteristics;

    while (ch) {
        num_of_entries += 2;  // 2 attributes for uuid and descriptor

        if (ch->cud_value) {
            num_of_entries++; // 1 attribute for cud
        }

        if ((ch->flags & BT_GATT_CHRC_NOTIFY) == BT_GATT_CHRC_NOTIFY) {
            num_of_entries++; // 1 attribute for ccc
        }

        ch = ch->next;
    }

   struct bt_gatt_attr *bt_attrs = zjs_malloc(sizeof(struct bt_gatt_attr) * num_of_entries);
    if (!bt_attrs) {
        PRINT("zjs_ble_register_service: out of memory allocating struct bt_gatt_attr\n");
        return false;
    }

    // populate the array
    memset(bt_attrs, 0, sizeof(struct bt_gatt_attr) * num_of_entries);

    // GATT Primary Service
    bt_attrs[entry_index].uuid = gatt_primary_service_uuid;
    bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
    bt_attrs[entry_index].read = bt_gatt_attr_read_service;
    bt_attrs[entry_index].user_data = service->uuid;
    entry_index++;

    ch = service->characteristics;
    while (ch) {
        // GATT Characteristic
        struct bt_gatt_chrc *chrc_user_data = zjs_malloc(sizeof(struct bt_gatt_chrc));
        if (!chrc_user_data) {
            PRINT("zjs_ble_register_service: out of memory allocating struct bt_gatt_chrc\n");
            return false;
        }

        memset(chrc_user_data, 0, sizeof(struct bt_gatt_chrc));

        chrc_user_data->uuid = ch->uuid;
        chrc_user_data->properties = ch->flags;
        bt_attrs[entry_index].uuid = gatt_characteristic_uuid;
        bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
        bt_attrs[entry_index].read = bt_gatt_attr_read_chrc;
        bt_attrs[entry_index].user_data = chrc_user_data;

        // TODO: handle multiple descriptors
        // DESCRIPTOR
        entry_index++;
        bt_attrs[entry_index].uuid = ch->uuid;
        if (ch->read_cb.id != -1) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_READ;
        }
        if (ch->write_cb.id != -1) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_WRITE;
        }
        bt_attrs[entry_index].read = zjs_ble_read_attr_callback;
        bt_attrs[entry_index].write = zjs_ble_write_attr_callback;
        bt_attrs[entry_index].user_data = ch;

        // hold references to the GATT attr for sending notification
        ch->chrc_attr = &bt_attrs[entry_index];
        entry_index++;

        // CUD
        if (ch->cud_value) {
            jerry_size_t sz = jerry_get_string_size(ch->cud_value);
            char *cud_buffer = zjs_malloc(sz+1);
            if (!cud_buffer) {
                PRINT("zjs_ble_register_service: out of memory allocating cud buffer\n");
                return false;
            }

            memset(cud_buffer, 0, sz+1);

            jerry_string_to_char_buffer(ch->cud_value, (jerry_char_t *)cud_buffer, sz);
            bt_attrs[entry_index].uuid = gatt_cud_uuid;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
            bt_attrs[entry_index].read = bt_gatt_attr_read_cud;
            bt_attrs[entry_index].user_data = cud_buffer;
            entry_index++;
        }

        // CCC
        if ((ch->flags & BT_GATT_CHRC_NOTIFY) == BT_GATT_CHRC_NOTIFY) {
            // add CCC only if notify flag is set
            struct _bt_gatt_ccc *ccc_user_data = zjs_malloc(sizeof(struct _bt_gatt_ccc));
            if (!ccc_user_data) {
                PRINT("zjs_ble_register_service: out of memory allocating struct bt_gatt_ccc\n");
                return false;
            }

            memset(ccc_user_data, 0, sizeof(struct _bt_gatt_ccc));

            ccc_user_data->cfg = ble_conn->blvl_ccc_cfg;
            ccc_user_data->cfg_len = ARRAY_SIZE(ble_conn->blvl_ccc_cfg);
            ccc_user_data->cfg_changed = zjs_ble_blvl_ccc_cfg_changed;
            bt_attrs[entry_index].uuid = gatt_ccc_uuid;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
            bt_attrs[entry_index].read = bt_gatt_attr_read_ccc;
            bt_attrs[entry_index].write = bt_gatt_attr_write_ccc;
            bt_attrs[entry_index].user_data = ccc_user_data;
            entry_index++;
        }

        ch = ch->next;
    }

    if (entry_index != num_of_entries) {
        PRINT("zjs_ble_register_service: number of entries didn't match\n");
        return false;
    }

    DBG_PRINT("Registered service: %d entries\n", entry_index);
    bt_gatt_register(bt_attrs, entry_index);
    return true;
}

static jerry_value_t zjs_ble_set_services(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc)
{
    // arg 0 should be an array of services
    // arg 1 is optionally an callback function
    if (argc < 1 ||
        !jerry_value_is_array(argv[0]) ||
        (argc > 1 && !jerry_value_is_function(argv[1]))) {
        return zjs_error("zjs_ble_set_services: invalid arguments");
    }

    // FIXME: currently hard-coded to work with demo
    // which has only 1 primary service and 2 characteristics
    // add support for multiple services
    jerry_value_t v_services = argv[0];
    int array_size = jerry_get_array_length(v_services);
    if (array_size == 0) {
        return zjs_error("zjs_ble_set_services: services array is empty");
    }

    // free existing services
    if (ble_conn->services) {
        zjs_ble_free_services(ble_conn->services);
        ble_conn->services = NULL;
    }

    bool success = true;
    ble_service_t *previous = NULL;
    for (int i = 0; i < array_size; i++) {
        jerry_value_t v_service = jerry_get_property_by_index(v_services, i);

        if (!jerry_value_is_object(v_service)) {
            return zjs_error("zjs_ble_set_services: service is not object");
        }

        ble_service_t *service = zjs_malloc(sizeof(ble_service_t));
        if (!service) {
            return zjs_error("zjs_ble_set_services: out of memory allocating ble_service_t");
        }

        memset(service, 0, sizeof(ble_service_t));

        service->service_obj = jerry_acquire_value(v_service);
        jerry_set_object_native_handle(service->service_obj,
                                       (uintptr_t)service, NULL);

        if (!zjs_ble_parse_service(service)) {
            return zjs_error("zjs_ble_set_services: failed to parse service");
        }

        if (!zjs_ble_register_service(service)) {
            success = false;
            break;
        }

        // append to the list
        if (!ble_conn->services) {
            ble_conn->services = service;
            previous = service;
        }
        else {
           previous->next = service;
        }
    }

    if (argc > 1) {
        jerry_value_t arg;
        arg = success ? ZJS_UNDEFINED :
              jerry_create_string((jerry_char_t *)"failed to register services");
        jerry_value_t rval = jerry_call_function(argv[1], ZJS_UNDEFINED, &arg, 1);
        if (jerry_value_has_error_flag(rval)) {
            PRINT("zjs_ble_set_services: failed to call callback function\n");
        }
    }

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_ble_update_rssi(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    // Todo: get actual RSSI value from Zephyr Bluetooth driver
    jerry_value_t arg = jerry_create_number(-50);
    zjs_trigger_event(ble_conn->ble_obj, "rssiUpdate", &arg, 1, NULL, NULL);
    return ZJS_UNDEFINED;
}

// Constructor
static jerry_value_t zjs_ble_primary_service(const jerry_value_t function_obj,
                                             const jerry_value_t this,
                                             const jerry_value_t argv[],
                                             const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_object(argv[0])) {
        return zjs_error("zjs_ble_primary_service: invalid arguments");
    }

    return jerry_acquire_value(argv[0]);
}

// Constructor
static jerry_value_t zjs_ble_characteristic(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_object(argv[0])) {
        return zjs_error("zjs_ble_characterstic: invalid arguments");
    }

    jerry_value_t obj = jerry_acquire_value(argv[0]);
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

    return argv[0];
}

// Constructor
static jerry_value_t zjs_ble_descriptor(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_object(argv[0])) {
        return zjs_error("zjs_ble_descriptor: invalid arguments");
    }

    return jerry_acquire_value(argv[0]);
}

jerry_value_t zjs_ble_init()
{
    nano_sem_init(&zjs_ble_nano_sem);

    // create global BLE object
    jerry_value_t ble_obj = jerry_create_object();
    zjs_obj_add_function(ble_obj, zjs_ble_disconnect, "disconnect");
    zjs_obj_add_function(ble_obj, zjs_ble_start_advertising, "startAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_stop_advertising, "stopAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_set_services, "setServices");
    zjs_obj_add_function(ble_obj, zjs_ble_update_rssi, "updateRssi");

    // register constructors
    zjs_obj_add_function(ble_obj, zjs_ble_primary_service, "PrimaryService");
    zjs_obj_add_function(ble_obj, zjs_ble_characteristic, "Characteristic");
    zjs_obj_add_function(ble_obj, zjs_ble_descriptor, "Descriptor");

    // make it an event object
    zjs_make_event(ble_obj);

    // bt events are called from the FIBER context, since we can't call
    // zjs_trigger_event() directly, we need to register a c callback which
    // the C callback will call zjs_trigger_event()
    ble_conn->ready_cb.id = zjs_add_c_callback(ble_conn, zjs_ble_ready_c_callback);
    ble_conn->connected_cb.id = zjs_add_c_callback(ble_conn, zjs_ble_connected_c_callback);
    ble_conn->disconnected_cb.id = zjs_add_c_callback(ble_conn, zjs_ble_disconnected_c_callback);
    ble_conn->ble_obj = ble_obj;

    return ble_obj;
}
#endif  // QEMU_BUILD
#endif  // BUILD_MODULE_BLE
