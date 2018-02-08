// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_BLE
#ifndef QEMU_BUILD
// C includes
#include <stdlib.h>
#include <string.h>

// Zephyr includes
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <zephyr.h>

// ZJS includes
#include "zjs_ble.h"
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_util.h"

#define ZJS_BLE_UUID_LEN                       36

#define ZJS_BLE_RESULT_SUCCESS                 0x00
#define ZJS_BLE_RESULT_INVALID_OFFSET          BT_ATT_ERR_INVALID_OFFSET
#define ZJS_BLE_RESULT_ATTR_NOT_LONG           BT_ATT_ERR_ATTRIBUTE_NOT_LONG
#define ZJS_BLE_RESULT_INVALID_ATTRIBUTE_LEN   BT_ATT_ERR_INVALID_ATTRIBUTE_LEN
#define ZJS_BLE_RESULT_UNLIKELY_ERROR          BT_ATT_ERR_UNLIKELY

#define ZJS_BLE_TIMEOUT_TICKS                  5000

typedef void (*ccc_cfg_changed_func)(const struct bt_gatt_attr *attr,
                                     u16_t value);

typedef struct ble_buffer_handle {
    zjs_callback_id id;
    jerry_value_t js_callback;
    const void *buffer;
    u16_t buffer_size;
    u16_t offset;
    u32_t error_code;
} ble_buffer_handle_t;

typedef struct ble_notify_handle {
    zjs_callback_id id;
    jerry_value_t js_callback;
} ble_notify_handle_t;

typedef struct zjs_ble_characteristic {
    int flags;
    jerry_value_t chrc_obj;
    struct bt_uuid *uuid;
    struct bt_gatt_attr *chrc_attr;
    struct bt_gatt_attr *ccc_attr;
    jerry_value_t cud_value;
    ble_buffer_handle_t read_cb;
    ble_buffer_handle_t write_cb;
    ble_notify_handle_t subscribe_cb;
    ble_notify_handle_t unsubscribe_cb;
    ble_notify_handle_t notify_cb;
    ccc_cfg_changed_func ccc_cfg_changed;
    struct zjs_ble_characteristic *next;
} ble_characteristic_t;

typedef struct zjs_ble_service {
    jerry_value_t service_obj;
    struct bt_uuid *uuid;
    ble_characteristic_t *characteristics;
    struct zjs_ble_service *next;
    struct bt_gatt_service svc;
} ble_service_t;

typedef struct zjs_ble_connection {
    struct bt_conn *bt_conn;
    struct zjs_ble_connection *next;
} ble_connection_t;

typedef struct zjs_ble_handle {
    struct bt_gatt_ccc_cfg blvl_ccc_cfg[CONFIG_BT_MAX_PAIRED];
    jerry_value_t ble_obj;
    ble_service_t *services;
    ble_connection_t *connections;
} ble_handle_t;

static struct bt_uuid *gatt_primary_service_uuid =
    BT_UUID_DECLARE_16(BT_UUID_GATT_PRIMARY_VAL);
static struct bt_uuid *gatt_characteristic_uuid =
    BT_UUID_DECLARE_16(BT_UUID_GATT_CHRC_VAL);
static struct bt_uuid *gatt_cud_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_CUD_VAL);
static struct bt_uuid *gatt_ccc_uuid = BT_UUID_DECLARE_16(BT_UUID_GATT_CCC_VAL);

static struct k_sem ble_sem;

static ble_handle_t *ble_handle = NULL;

static const jerry_object_native_info_t ble_type_info = {
   .free_cb = free_handle_nop
};

struct bt_uuid *zjs_ble_new_uuid_16(u16_t value)
{
    struct bt_uuid_16 *uuid = zjs_malloc(sizeof(struct bt_uuid_16));
    if (!uuid) {
        ERR_PRINT("out of memory allocating struct bt_uuid_16\n");
        return NULL;
    }

    memset(uuid, 0, sizeof(struct bt_uuid_16));

    uuid->uuid.type = BT_UUID_TYPE_16;
    uuid->val = value;
    return (struct bt_uuid *)uuid;
}

static ble_connection_t *zjs_ble_new_connection(ble_connection_t **conns,
                                                struct bt_conn *conn)
{
    ble_connection_t *new_conn = zjs_malloc(sizeof(ble_connection_t));
    if (!new_conn) {
        ERR_PRINT("cannot allocate ble_connection_t\n");
        return NULL;
    }

    memset(new_conn, 0, sizeof(ble_connection_t));
    new_conn->bt_conn = conn;
    new_conn->next = *conns;
    *conns = new_conn;
    return new_conn;
}

static void zjs_ble_release_connection(ble_connection_t **conns,
                                       ble_connection_t *conn)
{
    if (!conns || !conn)
        return;

    if (*conns == conn) {
        // if conn is the head
        ble_connection_t *tmp = *conns;
        *conns = (*conns)->next;
        bt_conn_unref(tmp->bt_conn);
        zjs_free(tmp);
        return;
    }

    ble_connection_t *current = (*conns)->next;
    ble_connection_t *previous = *conns;
    while (current && previous) {
        if (current == conn) {
            ble_connection_t *tmp = current;
            previous->next = current->next;
            bt_conn_unref(tmp->bt_conn);
            zjs_free(tmp);
            return;
        }

        previous = current;
        current = current->next;
    }
}

static void zjs_ble_free_characteristics(ble_characteristic_t *chrc)
{
    ble_characteristic_t *tmp;
    while (chrc) {
        tmp = chrc;
        chrc = chrc->next;

        jerry_release_value(tmp->chrc_obj);

        if (tmp->cud_value) {
            jerry_release_value(tmp->cud_value);
        }

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
    while (service) {
        tmp = service;
        service = service->next;

        jerry_release_value(tmp->service_obj);

        if (tmp->uuid)
            zjs_free(tmp->uuid);
        if (tmp->characteristics)
            zjs_ble_free_characteristics(tmp->characteristics);
        if (tmp->svc.attrs)
            zjs_free(tmp->svc.attrs);

        zjs_free(tmp);
    }
}

static ZJS_DECL_FUNC(zjs_ble_read_callback_function)
{
    // TODO: couldn't use ZJS_VALIDATE_ARGS here because it needs to give the
    //   semaphore on error case
    if (argc != 2 || !jerry_value_is_number(argv[0]) ||
        !zjs_value_is_buffer(argv[1])) {
        k_sem_give(&ble_sem);
        return zjs_error("invalid arguments");
    }

    ZJS_GET_HANDLE(function_obj, struct zjs_ble_characteristic, chrc,
                   ble_type_info);

    chrc->read_cb.error_code = (u32_t)jerry_get_number_value(argv[0]);

    zjs_buffer_t *buf = zjs_buffer_find(argv[1]);
    if (buf) {
        chrc->read_cb.buffer = buf->buffer;
        chrc->read_cb.buffer_size = buf->bufsize;
    } else {
        ERR_PRINT("buffer not found\n");
    }

    // unblock fiber
    k_sem_give(&ble_sem);
    return ZJS_UNDEFINED;
}

static void zjs_ble_read_c_callback(void *handle, const void *argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_buffer_handle_t *cb = &chrc->read_cb;

    ZVAL offset = jerry_create_number(cb->offset);
    ZVAL callback =
        jerry_create_external_function(zjs_ble_read_callback_function);

    jerry_set_object_native_pointer(callback, handle, &ble_type_info);

    jerry_value_t args[2] = { offset, callback };
    ZVAL rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        DBG_PRINT("failed to call onReadRequest function\n");
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static ssize_t zjs_ble_read_attr_callback(struct bt_conn *conn,
                                          const struct bt_gatt_attr *attr,
                                          void *buf, u16_t len, u16_t offset)
{
    if (offset > len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    ble_characteristic_t *chrc = attr->user_data;

    if (!chrc) {
        ERR_PRINT("characteristic not found\n");
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
        if (k_sem_take(&ble_sem, ZJS_BLE_TIMEOUT_TICKS)) {
            ERR_PRINT("JS callback timed out\n");
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }

        if (chrc->read_cb.error_code == ZJS_BLE_RESULT_SUCCESS) {
            if (chrc->read_cb.buffer && chrc->read_cb.buffer_size > 0) {
                // buffer should point to the Buffer object that JS created
                // copy the bytes into the return buffer ptr
                memcpy(buf, chrc->read_cb.buffer, chrc->read_cb.buffer_size);
                return chrc->read_cb.buffer_size;
            }

            ERR_PRINT("buffer is empty\n");
            return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
        } else {
            ERR_PRINT("on read attr error %u\n",
                      (unsigned int)chrc->read_cb.error_code);
            return BT_GATT_ERR(chrc->read_cb.error_code);
        }
    }

    DBG_PRINT("js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static ZJS_DECL_FUNC(zjs_ble_write_callback_function)
{
    // TODO: couldn't use ZJS_VALIDATE_ARGS here because it needs to give the
    //   semaphore on error case
    if (argc != 1 || !jerry_value_is_number(argv[0])) {
        k_sem_give(&ble_sem);
        return zjs_error("invalid arguments");
    }

    ZJS_GET_HANDLE(function_obj, struct zjs_ble_characteristic, chrc,
                   ble_type_info);

    // store the return value in the write_cb struct
    chrc->write_cb.error_code = (u32_t)jerry_get_number_value(argv[0]);

    // unblock fiber
    k_sem_give(&ble_sem);
    return ZJS_UNDEFINED;
}

static void zjs_ble_write_c_callback(void *handle, const void *argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_buffer_handle_t *cb = &chrc->write_cb;

    ZVAL_MUTABLE buf_obj;
    if (cb->buffer && cb->buffer_size > 0) {
        zjs_buffer_t *buf;
        buf_obj = zjs_buffer_create(cb->buffer_size, &buf);
        if (buf) {
            memcpy(buf->buffer, cb->buffer, cb->buffer_size);
        } else {
            // can't pass object with error flag as a JS arg
            jerry_value_clear_error_flag(&buf_obj);
        }
    } else {
        buf_obj = jerry_create_null();
    }

    ZVAL offset = jerry_create_number(cb->offset);
    // TODO: support withoutResponse flag
    ZVAL without_response = jerry_create_boolean(false);
    ZVAL callback =
        jerry_create_external_function(zjs_ble_write_callback_function);

    jerry_set_object_native_pointer(callback, handle, &ble_type_info);

    jerry_value_t args[4] = { buf_obj, offset, without_response, callback };
    ZVAL rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 4);
    if (jerry_value_has_error_flag(rval)) {
        DBG_PRINT("failed to call onWriteRequest function\n");
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static ssize_t zjs_ble_write_attr_callback(struct bt_conn *conn,
                                           const struct bt_gatt_attr *attr,
                                           const void *buf, u16_t len,
                                           u16_t offset, u8_t flags)
{
    ble_characteristic_t *chrc = attr->user_data;

    if (!chrc) {
        ERR_PRINT("characteristic not found\n");
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
        if (k_sem_take(&ble_sem, ZJS_BLE_TIMEOUT_TICKS)) {
            ERR_PRINT("JS callback timed out\n");
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }

        if (chrc->write_cb.error_code == ZJS_BLE_RESULT_SUCCESS) {
            return len;
        } else {
            return BT_GATT_ERR(chrc->write_cb.error_code);
        }
    }

    DBG_PRINT("js callback not available\n");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
}

static ZJS_DECL_FUNC(zjs_ble_update_value_callback_function)
{
    // args: buffer
    ZJS_VALIDATE_ARGS(Z_BUFFER);

    ZJS_GET_HANDLE(this, struct zjs_ble_characteristic, chrc, ble_type_info);

    if (chrc->chrc_attr) {
        zjs_buffer_t *buf = zjs_buffer_find(argv[0]);
        if (buf) {
            // loop through all the connections
            ble_connection_t *conn = ble_handle->connections;
            while (conn) {
                bt_gatt_notify(conn->bt_conn, chrc->chrc_attr, buf->buffer,
                               buf->bufsize);
                conn = conn->next;
            }
        }
    }

    return ZJS_UNDEFINED;
}

static void zjs_ble_subscribe_c_callback(void *handle, const void *argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->subscribe_cb;

    ZVAL max_size = jerry_create_number(20);
    ZVAL callback =
        jerry_create_external_function(zjs_ble_update_value_callback_function);

    jerry_value_t args[2] = { max_size, callback };
    ZVAL rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, args, 2);
    if (jerry_value_has_error_flag(rval)) {
        DBG_PRINT("failed to call onSubscribe function\n");
    }
}

static void zjs_ble_unsubscribe_c_callback(void *handle, const void *argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->unsubscribe_cb;

    ZVAL rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, NULL, 0);
    if (jerry_value_has_error_flag(rval)) {
        DBG_PRINT("failed to call onUnsubscribe function\n");
    }
}

static void zjs_ble_notify_c_callback(void *handle, const void *argv)
{
    ble_characteristic_t *chrc = (ble_characteristic_t *)handle;
    ble_notify_handle_t *cb = &chrc->notify_cb;

    ZVAL rval = jerry_call_function(cb->js_callback, chrc->chrc_obj, NULL, 0);
    if (jerry_value_has_error_flag(rval)) {
        DBG_PRINT("failed to call onNotify function\n");
    }
}

static ble_characteristic_t *get_base_chrc(const struct bt_gatt_attr *attr)
{
    ble_service_t *service;
    ble_characteristic_t *chrc;

    for (service = ble_handle->services; service; service = service->next) {
        for (chrc = service->characteristics; chrc; chrc = chrc->next) {
            if (chrc->ccc_attr == attr) {
                return chrc;
            }
        }
    }

    return NULL;
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static void zjs_ble_blvl_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                         u16_t value)
{
    ble_characteristic_t *base_chrc = get_base_chrc(attr);

    if (base_chrc) {
        if (base_chrc->subscribe_cb.id != -1 && value == 1) {
            DBG_PRINT("client subscribed for notification\n");
            zjs_signal_callback(base_chrc->subscribe_cb.id, NULL, 0);
        } else if (base_chrc->unsubscribe_cb.id != -1 && value == 0) {
            DBG_PRINT("client unsubscribed for notification\n");
            zjs_signal_callback(base_chrc->unsubscribe_cb.id, NULL, 0);
        }
    } else {
        ERR_PRINT("base characteristic not found\n");
    }
}

// a zjs_pre_emit callback
static bool string_arg(void *unused, jerry_value_t argv[], u32_t *argc,
                       const char *buffer, u32_t bytes)
{
    // requires: buffer contains string with bytes chars including null term
    argv[0] = jerry_create_string((jerry_char_t *)buffer);
    *argc = 1;
    return true;
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static void zjs_ble_connected(struct bt_conn *conn, u8_t err)
{
    if (err) {
        ERR_PRINT("Connection failed (err %u)\n", err);
    } else {
        DBG_PRINT("new connection\n");
        // FIXME: this should probably be a static pool to avoid malloc here,
        //   based on configured max connections
        ble_connection_t *new_conn =
            zjs_ble_new_connection(&ble_handle->connections, conn);
        if (!new_conn) {
            ERR_PRINT("failed to create new client connection\n");
            return;
        }

        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        new_conn->bt_conn = bt_conn_ref(conn);
        zjs_defer_emit_event(ble_handle->ble_obj, "accept", addr,
                             BT_ADDR_LE_STR_LEN, string_arg, zjs_release_args);

        DBG_PRINT("client connected: %s\n", addr);
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
static void zjs_ble_disconnected(struct bt_conn *conn, u8_t reason)
{
    ble_connection_t *ble_conn = ble_handle->connections;
    while (ble_conn) {
        if (ble_conn->bt_conn == conn) {
            char addr[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
            zjs_defer_emit_event(ble_handle->ble_obj, "disconnect", addr,
                                 BT_ADDR_LE_STR_LEN, string_arg,
                                 zjs_release_args);
            zjs_ble_release_connection(&ble_handle->connections, ble_conn);
            DBG_PRINT("client disconnected (reason %u): %s\n", reason, addr);
            return;
        }
        ble_conn = ble_conn->next;
    }
    DBG_PRINT("connection not found\n");
}

static struct bt_conn_cb zjs_ble_conn_callbacks = {
    .connected = zjs_ble_connected,
    .disconnected = zjs_ble_disconnected,
};

static void zjs_ble_auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    DBG_PRINT("pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb zjs_ble_auth_cb_display = {
    .cancel = zjs_ble_auth_cancel,
};

void zjs_ble_emit_powered_event()
{
    const char state[] = "poweredOn";
    ZJS_ASSERT(ble_handle, "ble_handle not set");
    zjs_defer_emit_event(ble_handle->ble_obj, "stateChange", state,
                         sizeof(state), string_arg, zjs_release_args);
}

static ZJS_DECL_FUNC(zjs_ble_disconnect)
{
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = BT_ADDR_LE_STR_LEN;
    char addr[size];
    zjs_copy_jstring(argv[0], addr, &size);

    ble_connection_t *conn = ble_handle->connections;
    while (conn) {
        char bt_addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn->bt_conn), bt_addr,
                          sizeof(bt_addr));
        if (!strncmp(addr, bt_addr, BT_ADDR_LE_STR_LEN)) {
            int error = bt_conn_disconnect(conn->bt_conn,
                                           BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            if (error) {
                return zjs_error("ble disconnect failed");
            }
            return ZJS_UNDEFINED;
        }
        conn = conn->next;
    }

    return zjs_error("ble connection not found");
}

const int ZJS_SUCCESS = 0;
const int ZJS_URL_TOO_LONG = 1;
const int ZJS_ALLOC_FAILED = 2;
const int ZJS_URL_SCHEME_ERROR = 3;

static int zjs_encode_url_frame(jerry_value_t url, u8_t **frame, int *size)
{
    // requires: url is a URL string, frame points to a u8_t *, url contains
    //             only UTF-8 characters and hence no nil values
    //  effects: allocates a new buffer that will fit an Eddystone URL frame
    //             with a compressed version of the given url; returns it in
    //             *frame and returns the size of the frame in bytes in *size,
    //             and frame is then owned by the caller, to be freed later with
    //             zjs_free
    //  returns: 0 for success, 1 for URL too long, 2 for out of memory, 3 for
    //             invalid url scheme/syntax (only http:// or https:// allowed)

    // FIXME: this needs unit tests; there could easily be a bug especially
    //   regarding whether a final null terminator on the URL fits in the 17
    //   bytes or not; spec seems unclear on whether it's required

    // max len is https://www. and .info/ encoded in 2 bytes + 15 raw = 33
    const int MAX_URL_LENGTH = 33;

    jerry_size_t len = MAX_URL_LENGTH;
    char buf[len];
    zjs_copy_jstring(url, buf, &len);
    if (!len)
        return ZJS_URL_TOO_LONG;

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
    } else {
        offset += 4;
    }

    // FIXME: skipping the compression of .com, .com/, .org, etc for now

    len -= offset;
    if (len > 17)  // max URL length specified by Eddystone spec
        return ZJS_URL_TOO_LONG;

    u8_t *ptr = zjs_malloc(len + 5);
    if (!ptr)
        return ZJS_ALLOC_FAILED;

    ptr[0] = 0xaa;    // Eddystone UUID
    ptr[1] = 0xfe;    // Eddystone UUID
    ptr[2] = 0x10;    // Eddystone-URL frame type
    ptr[3] = 0x00;    // calibrated Tx power at 0m
    ptr[4] = scheme;  // encoded URL scheme prefix
    strncpy(ptr + 5, buf + offset, len);

    *size = len + 5;
    *frame = ptr;
    return ZJS_SUCCESS;
}

static ZJS_DECL_FUNC(zjs_ble_start_advertising)
{
    // arg 0 should be the device name to advertise, e.g. "Arduino101"
    // arg 1 should be an array of UUIDs (short, 4 hex chars)
    // arg 2 should be a short URL (typically registered with Google, I think)
    ZJS_VALIDATE_ARGS(Z_STRING, Z_ARRAY, Z_OPTIONAL Z_STRING);

    jerry_value_t array = argv[1];
    if (!jerry_value_is_array(array)) {
        return zjs_error("expected array");
    }

    const int MAX_NAME_LENGTH = 80;
    jerry_size_t size = MAX_NAME_LENGTH;
    char name[size];
    zjs_copy_jstring(argv[0], name, &size);
    if (!size)
        return zjs_error("name too long");

    struct bt_data sd[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, name, size),
    };

    /*
     * Set Advertisement data. Based on the Eddystone specification:
     * https://github.com/google/eddystone/blob/master/protocol-specification.md
     * https://github.com/google/eddystone/tree/master/eddystone-url
     */
    u8_t *url_frame = NULL;
    int frame_size;
    if (argc >= 3) {
        if (zjs_encode_url_frame(argv[2], &url_frame, &frame_size)) {
            DBG_PRINT("error encoding url frame, won't be advertised\n");

            // TODO: Make use of error values and turn them into exceptions
        }
    }

    u32_t arraylen = jerry_get_array_length(array);
    int records = arraylen;
    if (url_frame)
        records += 2;

    if (records == 0) {
        return zjs_error("nothing to advertise");
    }

    const u8_t url_adv[] = { 0xaa, 0xfe };

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

    for (int i = 0; i < arraylen; i++) {
        ZVAL uuid = jerry_get_property_by_index(array, i);
        if (!jerry_value_is_string(uuid)) {
            return zjs_error("invalid uuid argument type");
        }

        const int MAX_UUID_LENGTH = 4;
        jerry_size_t size = MAX_UUID_LENGTH + 1;
        char ubuf[size];
        zjs_copy_jstring(uuid, ubuf, &size);
        if (size != MAX_UUID_LENGTH) {
            ERR_PRINT("SIZE: %u\n", (unsigned int)size);
            return zjs_error("unexpected uuid length");
        }

        u8_t bytes[2];
        if (!zjs_hex_to_byte(ubuf + 2, &bytes[0]) ||
            !zjs_hex_to_byte(ubuf, &bytes[1])) {
            return zjs_error("invalid char in uuid");
        }

        ad[index].type = BT_DATA_UUID16_ALL;
        ad[index].data_len = 2;
        ad[index].data = bytes;
        index++;
    }

    // FIXME: If we're really running this synchronously, why are we sending
    //   back the results asynchronously through hoops?
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    jerry_value_t error;
    if (err) {
        error = zjs_error("advertising failed");
        jerry_value_clear_error_flag(&error);
    } else {
        error = jerry_create_null();
    }
    zjs_defer_emit_event(ble_handle->ble_obj, "advertisingStart", &error,
                         sizeof(jerry_value_t), zjs_copy_arg, zjs_release_args);
    DBG_PRINT("BLE event: advertisingStart\n");

    zjs_free(url_frame);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_ble_stop_advertising)
{
    DBG_PRINT("stopAdvertising has been called\n");
    return ZJS_UNDEFINED;
}

static bool zjs_ble_parse_characteristic(ble_characteristic_t *chrc)
{
    char uuid[ZJS_BLE_UUID_LEN];
    if (!chrc || !chrc->chrc_obj)
        return false;

    jerry_value_t chrc_obj = chrc->chrc_obj;
    if (!zjs_obj_get_string(chrc_obj, "uuid", uuid, ZJS_BLE_UUID_LEN)) {
        ERR_PRINT("characteristic uuid doesn't exist\n");
        return false;
    }

    chrc->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    ZVAL v_props = zjs_get_property(chrc_obj, "properties");
    if (!jerry_value_is_array(v_props)) {
        ERR_PRINT("properties is empty or not array\n");
        return false;
    }

    for (int i = 0; i < jerry_get_array_length(v_props); i++) {
        ZVAL v_property = jerry_get_property_by_index(v_props, i);

        if (!jerry_value_is_string(v_property)) {
            ERR_PRINT("property is not string\n");
            return false;
        }

        const int MAX_NAME_LENGTH = 20;
        jerry_size_t size = MAX_NAME_LENGTH;
        char name[MAX_NAME_LENGTH];
        zjs_copy_jstring(v_property, name, &size);

        if (strequal(name, "read")) {
            chrc->flags |= BT_GATT_CHRC_READ;
        } else if (strequal(name, "write")) {
            chrc->flags |= BT_GATT_CHRC_WRITE;
        } else if (strequal(name, "notify")) {
            chrc->flags |= BT_GATT_CHRC_NOTIFY;
        }
    }

    ZVAL v_descs = zjs_get_property(chrc_obj, "descriptors");
    if (!jerry_value_is_undefined(v_descs) &&
        !jerry_value_is_null(v_descs) &&
        !jerry_value_is_array(v_descs)) {
        ERR_PRINT("descriptors is not array\n");
        return false;
    }

    for (int i = 0; i < jerry_get_array_length(v_descs); i++) {
        ZVAL v_desc = jerry_get_property_by_index(v_descs, i);

        if (!jerry_value_is_object(v_desc)) {
            ERR_PRINT("not valid descriptor object\n");
            return false;
        }

        char desc_uuid[ZJS_BLE_UUID_LEN];
        if (!zjs_obj_get_string(v_desc, "uuid", desc_uuid, ZJS_BLE_UUID_LEN)) {
            ERR_PRINT("descriptor uuid doesn't exist\n");
            return false;
        }

        if (strtoul(desc_uuid, NULL, 16) == BT_UUID_GATT_CUD_VAL) {
            // Support CUD only, ignore all other type of descriptors
            ZVAL v_value = zjs_get_property(v_desc, "value");
            if (jerry_value_is_string(v_value)) {
                // transfer ownership, free cud_value later
                chrc->cud_value = jerry_acquire_value(v_value);
            }
        }
    }

    ZVAL v_read = zjs_get_property(chrc_obj, "onReadRequest");
    if (jerry_value_is_function(v_read)) {
        chrc->read_cb.js_callback = jerry_acquire_value(v_read);
        chrc->read_cb.id = zjs_add_c_callback(chrc, zjs_ble_read_c_callback);
    } else {
        chrc->read_cb.id = -1;
    }

    ZVAL v_write = zjs_get_property(chrc_obj, "onWriteRequest");
    if (jerry_value_is_function(v_write)) {
        chrc->write_cb.js_callback = jerry_acquire_value(v_write);
        chrc->write_cb.id = zjs_add_c_callback(chrc, zjs_ble_write_c_callback);
    } else {
        chrc->write_cb.id = -1;
    }

    ZVAL v_sub = zjs_get_property(chrc_obj, "onSubscribe");
    if (jerry_value_is_function(v_sub)) {
        chrc->subscribe_cb.js_callback = jerry_acquire_value(v_sub);
        chrc->subscribe_cb.id =
            zjs_add_c_callback(chrc, zjs_ble_subscribe_c_callback);
    } else {
        chrc->subscribe_cb.id = -1;
    }

    ZVAL v_unsub = zjs_get_property(chrc_obj, "onUnsubscribe");
    if (jerry_value_is_function(v_unsub)) {
        chrc->unsubscribe_cb.js_callback = jerry_acquire_value(v_unsub);
        chrc->unsubscribe_cb.id =
            zjs_add_c_callback(chrc, zjs_ble_unsubscribe_c_callback);
    } else {
        chrc->unsubscribe_cb.id = -1;
    }

    ZVAL v_notify = zjs_get_property(chrc_obj, "onNotify");
    if (jerry_value_is_function(v_notify)) {
        chrc->notify_cb.js_callback = jerry_acquire_value(v_notify);
        chrc->notify_cb.id =
            zjs_add_c_callback(chrc, zjs_ble_notify_c_callback);
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
        ERR_PRINT("service uuid doesn't exist\n");
        return false;
    }
    service->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    ZVAL v_chrcs = zjs_get_property(service_obj, "characteristics");
    if (!jerry_value_is_array(v_chrcs)) {
        ERR_PRINT("characteristics is empty or not array\n");
        return false;
    }

    char *errmsg = NULL;
    ble_characteristic_t *previous = NULL;
    for (int i = 0; i < jerry_get_array_length(v_chrcs); i++) {
        // FIXME: it would make sense to move these next few lines into
        //   parse_characteristic
        ZVAL v_chrc = jerry_get_property_by_index(v_chrcs, i);
        if (!jerry_value_is_object(v_chrc)) {
            errmsg = "characteristic is not object";
            break;
        }

        ble_characteristic_t *chrc = zjs_malloc(sizeof(ble_characteristic_t));
        if (!chrc) {
            errmsg = "out of memory allocating ble_characteristic_t";
            break;
        }

        memset(chrc, 0, sizeof(ble_characteristic_t));
        chrc->read_cb.id = chrc->write_cb.id
                         = chrc->subscribe_cb.id
                         = chrc->unsubscribe_cb.id
                         = chrc->notify_cb.id
                         = -1;

        // transfer ownership of v_chrc to chrc struct
        chrc->chrc_obj = jerry_acquire_value(v_chrc);
        jerry_set_object_native_pointer(chrc->chrc_obj, chrc, &ble_type_info);

        // FIXME: All the stuff above this in the loop should move into
        //   parse_characteristic and it should take chrc_obj instead; it would
        //   reduce a lot of this error handling.
        if (!zjs_ble_parse_characteristic(chrc)) {
            jerry_release_value(chrc->chrc_obj);
            zjs_free(chrc);
            errmsg = "failed to parse temp characteristic";
            break;
        }

        // append to the list
        if (!service->characteristics) {
            service->characteristics = chrc;
            previous = chrc;
        } else {
            previous->next = chrc;
        }
    }

    if (errmsg) {
        ERR_PRINT("%s\n", errmsg);
        return false;
    }

    return true;
}

static bool zjs_ble_register_service(ble_service_t *service)
{
    if (!service) {
        ERR_PRINT("invalid ble_service\n");
        return false;
    }

    // FIXME: it appears this is leaking malloced buffers like a sieve on error

    // calculate the number of GATT attributes to allocate
    int entry_index = 0;
    int num_of_entries = 1;    // 1 attribute for service uuid
    ble_characteristic_t *ch = service->characteristics;

    while (ch) {
        num_of_entries += 2;   // 2 attributes for uuid and descriptor

        if (ch->cud_value) {
            num_of_entries++;  // 1 attribute for cud
        }

        if ((ch->flags & BT_GATT_CHRC_NOTIFY) == BT_GATT_CHRC_NOTIFY) {
            num_of_entries++;  // 1 attribute for ccc
        }

        ch = ch->next;
    }

    struct bt_gatt_attr *bt_attrs = zjs_malloc(sizeof(struct bt_gatt_attr) *
                                               num_of_entries);
    if (!bt_attrs) {
        ERR_PRINT("out of memory\n");
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
        struct bt_gatt_chrc *chrc_user_data =
            zjs_malloc(sizeof(struct bt_gatt_chrc));
        if (!chrc_user_data) {
            ERR_PRINT("out of memory allocating struct bt_gatt_chrc\n");
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
            // FIXME: not sure what this value should be
            const int MAX_CUD_LENGTH = 64;
            jerry_size_t size = MAX_CUD_LENGTH;
            char *cud_buffer = zjs_alloc_from_jstring(ch->cud_value, &size);
            if (!cud_buffer) {
                ERR_PRINT("out of memory allocating cud buffer\n");
                return false;
            }

            // FIXME: cud_buffer should be freed later if no longer needed, but
            //   doesn't seem to be

            bt_attrs[entry_index].uuid = gatt_cud_uuid;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ;
            bt_attrs[entry_index].read = bt_gatt_attr_read_cud;
            bt_attrs[entry_index].user_data = cud_buffer;
            entry_index++;
        }

        // CCC
        if ((ch->flags & BT_GATT_CHRC_NOTIFY) == BT_GATT_CHRC_NOTIFY) {
            // add CCC only if notify flag is set
            struct _bt_gatt_ccc *ccc_user_data =
                zjs_malloc(sizeof(struct _bt_gatt_ccc));
            if (!ccc_user_data) {
                ERR_PRINT("out of memory allocating struct bt_gatt_ccc\n");
                return false;
            }

            memset(ccc_user_data, 0, sizeof(struct _bt_gatt_ccc));

            ccc_user_data->cfg = ble_handle->blvl_ccc_cfg;
            ccc_user_data->cfg_len = ARRAY_SIZE(ble_handle->blvl_ccc_cfg);
            ccc_user_data->cfg_changed = zjs_ble_blvl_ccc_cfg_changed;
            bt_attrs[entry_index].uuid = gatt_ccc_uuid;
            bt_attrs[entry_index].perm = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
            bt_attrs[entry_index].read = bt_gatt_attr_read_ccc;
            bt_attrs[entry_index].write = bt_gatt_attr_write_ccc;
            bt_attrs[entry_index].user_data = ccc_user_data;

            // keep reference to ccc attrb
            ch->ccc_attr = &bt_attrs[entry_index];
            entry_index++;
        }

        ch = ch->next;
    }

    if (entry_index != num_of_entries) {
        ERR_PRINT("number of entries didn't match\n");
        return false;
    }

    DBG_PRINT("registered service: %d entries\n", entry_index);
    // keep reference to the bt attributes so we can free it
    service->svc.attrs = bt_attrs;
    service->svc.attr_count = entry_index;

    bt_gatt_service_register(&service->svc);
    return true;
}

static ZJS_DECL_FUNC(zjs_ble_set_services)
{
    // arg 0 should be an array of services
    // arg 1 is optionally an callback function
    ZJS_VALIDATE_ARGS(Z_ARRAY, Z_OPTIONAL Z_FUNCTION);

    jerry_value_t v_services = argv[0];
    int array_size = jerry_get_array_length(v_services);
    if (array_size == 0) {
        return zjs_error("services array is empty");
    }

    // free existing services
    if (ble_handle->services) {
        zjs_ble_free_services(ble_handle->services);
        ble_handle->services = NULL;
    }

    bool success = true;
    ble_service_t *previous = NULL;
    for (int i = 0; i < array_size; i++) {
        ZVAL v_service = jerry_get_property_by_index(v_services, i);

        if (!jerry_value_is_object(v_service)) {
            return zjs_error("service is not object");
        }

        ble_service_t *service = zjs_malloc(sizeof(ble_service_t));
        if (!service) {
            return zjs_error("out of memory allocating ble_service_t");
        }

        memset(service, 0, sizeof(ble_service_t));

        // transfer ownership of v_service to service struct
        service->service_obj = jerry_acquire_value(v_service);
        jerry_set_object_native_pointer(service->service_obj, service,
                                        &ble_type_info);

        // FIXME: The parse_service should take the service_obj instead and do
        //   all this above stuff in the loop, reducing error handling cruft.
        if (!zjs_ble_parse_service(service)) {
            jerry_release_value(service->service_obj);
            zjs_free(service);
            return zjs_error("failed to parse service");
        }

        if (!zjs_ble_register_service(service)) {
            // FIXME: it's very weird that this one error case is handled by
            //   calling an error callback, but the rest weren't? There seems
            //   to be no point to doing this async, again.
            success = false;
            break;
        }

        // append to the list
        if (!ble_handle->services) {
            ble_handle->services = service;
            previous = service;
        } else {
            previous->next = service;
        }
    }

    if (argc > 1) {
        ZVAL arg = success ? ZJS_UNDEFINED
                           : jerry_create_string("failed to register services");
        ZVAL rval = jerry_call_function(argv[1], ZJS_UNDEFINED, &arg, 1);
        if (jerry_value_has_error_flag(rval)) {
            DBG_PRINT("failed to call callback function\n");
        }
    }

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_ble_update_rssi)
{
    // TODO: get actual RSSI value from Zephyr Bluetooth driver
    jerry_value_t arg = jerry_create_number(-50);
    zjs_defer_emit_event(ble_handle->ble_obj, "rssiUpdate", &arg,
                         sizeof(jerry_value_t), zjs_copy_arg, zjs_release_args);
    return ZJS_UNDEFINED;
}

// Constructor
static ZJS_DECL_FUNC(zjs_ble_primary_service)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    return jerry_acquire_value(argv[0]);
}

// Constructor
static ZJS_DECL_FUNC(zjs_ble_characteristic)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    // FIXME: Taking over the incoming object isn't really right; one side
    //   effect will be if they've stored a reference to the object they
    //   passed in, they'll see these changes to it. So eventually we should
    //   change it. We should make sure this even works because the "this"
    //   object will be the newly constructed object and I believe that's
    //   really what we should be modifying. In the new zjs_error.c code I
    //   think I've successfully implemented a fully functional constructor
    //   as an example.
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

    val = jerry_create_number(ZJS_BLE_RESULT_INVALID_ATTRIBUTE_LEN);
    zjs_set_property(obj, "RESULT_INVALID_ATTRIBUTE_LENGTH", val);
    jerry_release_value(val);

    val = jerry_create_number(ZJS_BLE_RESULT_UNLIKELY_ERROR);
    zjs_set_property(obj, "RESULT_UNLIKELY_ERROR", val);
    jerry_release_value(val);

    return obj;
}

// Constructor
static ZJS_DECL_FUNC(zjs_ble_descriptor)
{
    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    return jerry_acquire_value(argv[0]);
}

static void zjs_ble_cleanup(void *native)
{
    ble_connection_t *conn = ble_handle->connections;
    while (conn) {
        ble_connection_t *tmp = conn;
        bt_conn_unref(tmp->bt_conn);
        conn = conn->next;
        zjs_free(tmp);
    }
    zjs_ble_free_services(ble_handle->services);
    jerry_release_value(ble_handle->ble_obj);
    zjs_free(ble_handle);
    ble_handle = NULL;
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or release prints!
void ble_bt_ready(int err)
{
    DBG_PRINT("bt_ready() is called [err %d]\n", err);
    zjs_ble_emit_powered_event();
}

static jerry_value_t zjs_ble_init()
{
    k_sem_init(&ble_sem, 0, 1);

    ble_handle_t *handle = zjs_malloc(sizeof(ble_handle_t));
    if (!handle) {
        return zjs_error_context("failed to allocate ble_handle", 0, 0);
    }
    memset(handle, 0, sizeof(ble_handle_t));

    // create global BLE object
    jerry_value_t ble_obj = zjs_create_object();
    zjs_obj_add_function(ble_obj, "disconnect", zjs_ble_disconnect);
    zjs_obj_add_function(ble_obj, "startAdvertising",
                         zjs_ble_start_advertising);
    zjs_obj_add_function(ble_obj, "stopAdvertising", zjs_ble_stop_advertising);
    zjs_obj_add_function(ble_obj, "setServices", zjs_ble_set_services);
    zjs_obj_add_function(ble_obj, "updateRssi", zjs_ble_update_rssi);

    // register constructors
    zjs_obj_add_function(ble_obj, "PrimaryService", zjs_ble_primary_service);
    zjs_obj_add_function(ble_obj, "Characteristic", zjs_ble_characteristic);
    zjs_obj_add_function(ble_obj, "Descriptor", zjs_ble_descriptor);

    // make it an emitter object
    zjs_make_emitter(ble_obj, ZJS_UNDEFINED, NULL, zjs_ble_cleanup);

    handle->ble_obj = ble_obj;

    // setup connection callbacks
    bt_conn_cb_register(&zjs_ble_conn_callbacks);
    bt_conn_auth_cb_register(&zjs_ble_auth_cb_display);

    ble_handle = handle;

#ifdef ZJS_ASHELL
    if (bt_enable(ble_bt_ready)) {
        ERR_PRINT("Failed to enable Bluetooth and may not be enabled again, " \
                  "please reboot\n");
    }
#endif
    return ble_obj;
}

JERRYX_NATIVE_MODULE(ble, zjs_ble_init)
#endif  // QEMU_BUILD
#endif  // BUILD_MODULE_BLE
