// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <string.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#include <misc/printk.h>
#define PRINT           printk

// ZJS includes
#include "zjs_ble.h"
#include "zjs_util.h"

#define DEVICE_NAME             "Arduino101"
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

/*
 * Set Advertisement data. Based on the Eddystone specification:
 * https://github.com/google/eddystone/blob/master/protocol-specification.md
 * https://github.com/google/eddystone/tree/master/eddystone-url
 */
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
        BT_DATA_BYTES(BT_DATA_SVC_DATA16,
                      0xaa, 0xfe, /* Eddystone UUID */
                      0x10, /* Eddystone-URL frame type */
                      0x00, /* Calibrated Tx power at 0m */
                      0x03, /* URL Scheme Prefix https://. */
                                        'g', 'o', 'o', '.', 'g', 'l', '/',
                                        '9', 'F', 'o', 'm', 'Q', 'C'),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x00, 0xfc)
};


static const struct bt_data sd[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

struct bt_conn *default_conn;

static void connected(struct bt_conn *conn, uint8_t err)
{
    PRINT("========= connected ========\n");
    if (err) {
        PRINT("Connection failed (err %u)\n", err);
    } else {
        default_conn = bt_conn_ref(conn);
        PRINT("Connected\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    PRINT("Disconnected (reason %u)\n", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static void auth_cancel(struct bt_conn *conn)
{
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        PRINT("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
        .cancel = auth_cancel,
};

struct ble_event_list_item {
    char event_type[20];
    struct zjs_callback zjs_cb;
    struct ble_event_list_item *next;
};

static struct ble_event_list_item *ble_event_list = NULL;

static struct ble_event_list_item *zjs_ble_event_callback_alloc()
{
    // effects: allocates a new callback list item and adds it to the list
    struct ble_event_list_item *item;
    PRINT ("Size of ble_event_list_item = %d\n", sizeof(struct ble_event_list_item));
    item = task_malloc(sizeof(struct ble_event_list_item));
    if (!item) {
        PRINT("error: out of memory allocating callback struct\n");
        return NULL;
    }

    item->next = ble_event_list;
    ble_event_list = item;
    return item;
}

static void dispatch(char *type, jerry_value_t args_p[], uint16_t args_cnt)
{
    struct ble_event_list_item *ev = ble_event_list;
    int len = strlen(type);
    while (ev)
    {
        if (!strncmp(ev->event_type, type, len))
        {
            jerry_value_t rval;
            jerry_call_function(ev->zjs_cb.js_callback, NULL, &rval, args_p, args_cnt);
            return;
        }
        ev = ev->next;
    }
}

static void queueDispatch(char *type, zjs_cb_wrapper_t func)
{
    // requires: called only from task context
    //  effects: finds the first callback for the given type and queues it up
    //             to run func to execute it at the next opportunity
    struct ble_event_list_item *ev = ble_event_list;
    int len = strlen(type);
    while (ev) {
        if (!strncmp(ev->event_type, type, len)) {
            ev->zjs_cb.call_function = func;
            zjs_queue_callback(&ev->zjs_cb);
            return;
        }
        ev = ev->next;
    }
}

static void zjs_bt_ready_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context
    //  effects: handles execution of the bt ready JS callback
    jerry_value_t rval, arg;
    zjs_init_api_value_string(&arg, "poweredOn");
    if (jerry_call_function(cb->js_callback, NULL, &rval, &arg, 1))
        jerry_release_value(&rval);
    jerry_release_value(&arg);
}

static void zjs_bt_ready(int err)
{
    if (!ble_event_list)
    {
        PRINT("zjs_bt_ready: no event handlers present\n");
        return;
    }
    PRINT("zjs_bt_ready is called [err %d]\n", err);

    queueDispatch("stateChange", zjs_bt_ready_call_function);
}

jerry_object_t *zjs_ble_init()
{
    // create global BLE object
    jerry_object_t *ble_obj = jerry_create_object();
    zjs_obj_add_function(ble_obj, zjs_ble_enable, "enable");
    zjs_obj_add_function(ble_obj, zjs_ble_on, "on");
    zjs_obj_add_function(ble_obj, zjs_ble_adv_start, "startAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_adv_stop, "stopAdvertising");
    return ble_obj;
}

bool zjs_ble_on(const jerry_object_t *function_obj_p,
                const jerry_value_t *this_p,
                jerry_value_t *ret_val_p,
                const jerry_value_t args_p[],
                const jerry_length_t args_cnt)
{
    if (args_cnt < 2 || args_p[0].type != JERRY_DATA_TYPE_STRING ||
                        args_p[1].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_on: invalid arguments\n");
        return false;
    }

    struct ble_event_list_item *item = zjs_ble_event_callback_alloc();
    if (!item)
        return false;

    char event[20];
    jerry_value_t arg = args_p[0];
    jerry_size_t sz = jerry_get_string_size(arg.u.v_string);
    // FIXME: need to make sure event name is less than 20 characters.
    // assert(sz < 20);
    int len = jerry_string_to_char_buffer(arg.u.v_string, (jerry_char_t *)event, sz);
    event[len] = '\0';
    PRINT ("\nEVENT TYPE: %s (%d)\n", event, len);

    item->zjs_cb.js_callback = jerry_acquire_object(args_p[1].u.v_object);
    memcpy(item->event_type, event, len);

    return true;
}

bool zjs_ble_enable(const jerry_object_t *function_obj_p,
                  const jerry_value_t *this_p,
                  jerry_value_t *ret_val_p,
                  const jerry_value_t args_p[],
                  const jerry_length_t args_cnt)
{
    PRINT ("====>About to enable the bluetooth\n");
    bt_enable(zjs_bt_ready);

    // setup connection callbacks
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(&auth_cb_display);

    return true;
}

bool zjs_ble_adv_start(const jerry_object_t *function_obj_p,
                      const jerry_value_t *this_p,
                      jerry_value_t *ret_val_p,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                            sd, ARRAY_SIZE(sd));

    PRINT("====>AdvertisingStarted..........\n");
    jerry_value_t args[1];
    args[0].type = JERRY_DATA_TYPE_UINT32 ;
    args[0].u.v_uint32  = (uint32_t) err;
    dispatch("advertisingStart", args, 1);
    return true;
}

bool zjs_ble_adv_stop(const jerry_object_t *function_obj_p,
                      const jerry_value_t *this_p,
                      jerry_value_t *ret_val_p,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt)
{
    PRINT ("stopAdvertising has been called\n");
    return true;
}
