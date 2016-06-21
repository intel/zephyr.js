// Copyright (c) 2016, Intel Corporation.

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
#include "zjs_util.h"

#define UUID_LEN                36

// Port this to javascript
#define BT_UUID_WEBBT BT_UUID_DECLARE_16(0xfc00)
#define BT_UUID_TEMP BT_UUID_DECLARE_16(0xfc0a)
#define BT_UUID_RGB BT_UUID_DECLARE_16(0xfc0b)
#define SENSOR_1_NAME "Temperature"
#define SENSOR_2_NAME "Led"

static struct bt_gatt_ccc_cfg  blvl_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
static uint8_t simulate_blvl;
static uint8_t rgb[3] = {0xff, 0x00, 0x00}; // red

struct bt_conn *default_conn;

struct ble_characteristic {
    struct bt_uuid *uuid;
    int flags;
    jerry_object_t *read_cb;
    jerry_object_t *write_cb;
    jerry_object_t *subscribe_cb;
    jerry_object_t *unsubscribe_cb;
    jerry_object_t *notify_cb;
    struct ble_characteristic *next;
};

struct ble_service {
    struct bt_uuid *uuid;
    struct ble_characteristic *characteristics;
    struct ble_service *next;
};

static struct ble_service zjs_primary_service = { NULL, NULL, NULL };

struct bt_uuid* zjs_ble_new_uuid_16(uint16_t value) {
    struct bt_uuid_16* uuid = task_malloc(sizeof(struct bt_uuid_16));
    if (!uuid) {
        PRINT("error: out of memory allocating struct bt_uuid_16\n");
        return NULL;
    }

    uuid->uuid.type = BT_UUID_TYPE_16;
    uuid->val = value;
    return (struct bt_uuid *) uuid;
}

static void zjs_ble_free_characteristics(struct ble_characteristic *chrc)
{
    struct ble_characteristic *tmp;
    while (chrc != NULL) {
        tmp = chrc;
        chrc = chrc->next;

        if (tmp->read_cb)
            jerry_release_object(tmp->read_cb);
        if (tmp->write_cb)
            jerry_release_object(tmp->write_cb);
        if (tmp->subscribe_cb)
            jerry_release_object(tmp->subscribe_cb);
        if (tmp->unsubscribe_cb)
            jerry_release_object(tmp->unsubscribe_cb);
        if (tmp->notify_cb)
            jerry_release_object(tmp->notify_cb);

        task_free(tmp);
    }
}

static ssize_t read_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset)
{
    PRINT("read_callback called\n");
    struct ble_characteristic* chrc = attr->user_data;

    if (!chrc) {
        PRINT("error: characteristic not found\n");
        return bt_gatt_attr_read(conn, attr, buf, len, offset, rgb,
               sizeof(rgb));
    }

    // FIXME
    // calling JS onReadRequest
    if (chrc->read_cb) {
        jerry_value_t rval;
        if (jerry_call_function(chrc->read_cb, NULL, &rval, NULL, 0))
            jerry_release_value(&rval);
    }

    // FIXME
    // return the value in the Buffer
    return bt_gatt_attr_read(conn, attr, buf, len, offset, rgb,
                 sizeof(rgb));
}

static ssize_t write_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    const void *buf, uint16_t len, uint16_t offset)
{
    // calling onWriteRequest() handler in js
    PRINT("write_callback called\n");
    struct ble_characteristic* chrc = attr->user_data;

    if (!chrc) {
        PRINT("error: characteristic not found\n");
        return 0;
    }

    // FIXME
    // calling JS onWriteRequest
    if (chrc->write_cb) {
        jerry_value_t rval;
        if (jerry_call_function(chrc->write_cb, NULL, &rval, NULL, 0))
            jerry_release_value(&rval);
    }

    // FIXME return
    return sizeof(rgb);
}

// Port this to javascript
static void blvl_ccc_cfg_changed(uint16_t value)
{
    simulate_blvl = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

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

struct zjs_ble_list_item {
    char event_type[20];
    struct zjs_callback zjs_cb;
    uint32_t intdata;
    struct zjs_ble_list_item *next;
};

static struct zjs_ble_list_item *zjs_ble_list = NULL;

static struct zjs_ble_list_item *zjs_ble_event_callback_alloc()
{
    // effects: allocates a new callback list item and adds it to the list
    struct zjs_ble_list_item *item;
    PRINT("Size of zjs_ble_list_item = %lu\n",
          sizeof(struct zjs_ble_list_item));
    item = task_malloc(sizeof(struct zjs_ble_list_item));
    if (!item) {
        PRINT("error: out of memory allocating callback struct\n");
        return NULL;
    }

    item->next = zjs_ble_list;
    zjs_ble_list = item;
    return item;
}

static void zjs_queue_dispatch(char *type, zjs_cb_wrapper_t func,
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

static void zjs_bt_ready_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context
    //  effects: handles execution of the bt ready JS callback
    jerry_value_t rval, arg;
    zjs_init_value_string(&arg, "poweredOn");
    if (jerry_call_function(cb->js_callback, NULL, &rval, &arg, 1))
        jerry_release_value(&rval);
    jerry_release_value(&arg);
}

static void zjs_bt_ready(int err)
{
    if (!zjs_ble_list) {
        PRINT("zjs_bt_ready: no event handlers present\n");
        return;
    }
    PRINT("zjs_bt_ready is called [err %d]\n", err);

    // FIXME: Probably we should return this err to JS like in adv_start?
    //   Maybe this wasn't in the bleno API?
    zjs_queue_dispatch("stateChange", zjs_bt_ready_call_function, 0);
}

jerry_object_t *zjs_ble_init()
{
    // create global BLE object
    jerry_object_t *ble_obj = jerry_create_object();
    zjs_obj_add_function(ble_obj, zjs_ble_enable, "enable");
    zjs_obj_add_function(ble_obj, zjs_ble_on, "on");
    zjs_obj_add_function(ble_obj, zjs_ble_adv_start, "startAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_adv_stop, "stopAdvertising");
    zjs_obj_add_function(ble_obj, zjs_ble_set_services, "setServices");

    // PrimaryService and Characteristic constructors
    zjs_obj_add_function(ble_obj, zjs_ble_primary_service, "PrimaryService");
    zjs_obj_add_function(ble_obj, zjs_ble_characteristic, "Characteristic");
    return ble_obj;
}

bool zjs_ble_on(const jerry_object_t *function_obj_p,
                const jerry_value_t *this_p,
                jerry_value_t *ret_val_p,
                const jerry_value_t args_p[],
                const jerry_length_t args_cnt)
{
    if (args_cnt < 2 || !ZJS_IS_STRING(args_p[0]) || !ZJS_IS_OBJ(args_p[1])) {
        PRINT("zjs_ble_on: invalid arguments\n");
        return false;
    }

    struct zjs_ble_list_item *item = zjs_ble_event_callback_alloc();
    if (!item)
        return false;

    char event[20];
    jerry_value_t arg = args_p[0];
    jerry_size_t sz = jerry_get_string_size(arg.u.v_string);
    // FIXME: need to make sure event name is less than 20 characters.
    // assert(sz < 20);
    int len = jerry_string_to_char_buffer(arg.u.v_string, (jerry_char_t *)event, sz);
    event[len] = '\0';
    PRINT("\nEVENT TYPE: %s (%d)\n", event, len);

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
    PRINT("About to enable the bluetooth, wait for bt_ready()...\n");
    bt_enable(zjs_bt_ready);

    // setup connection callbacks
    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(&auth_cb_display);

    return true;
}

static void zjs_bt_adv_start_call_function(struct zjs_callback *cb)
{
    // requires: called only from task context, expects intdata in cb to have
    //             been set previously
    //  effects: handles execution of the adv start JS callback
    struct zjs_ble_list_item *mycb = CONTAINER_OF(cb, struct zjs_ble_list_item,
                                                  zjs_cb);
    jerry_value_t rval, arg;
    arg.type = JERRY_DATA_TYPE_UINT32;
    arg.u.v_uint32 = mycb->intdata;
    if (jerry_call_function(cb->js_callback, NULL, &rval, &arg, 1))
        jerry_release_value(&rval);
    // int values don't really need to be released
}

bool zjs_ble_adv_start(const jerry_object_t *function_obj_p,
                       const jerry_value_t *this_p,
                       jerry_value_t *ret_val_p,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt)
{
    char name[80];
    char uuid[80];
    jerry_size_t sz;
    jerry_value_t v_uuid;

    if (args_cnt < 2 ||
        args_p[0].type != JERRY_DATA_TYPE_STRING ||
        args_p[1].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_adv_start: invalid arguments\n");
        return false;
    }

    jerry_get_array_index_value(args_p[1].u.v_object, 0, &v_uuid);

    if (v_uuid.type != JERRY_DATA_TYPE_STRING) {
        PRINT("zjs_ble_adv_start: invalid uuid argument type\n");
        return false;
    }

    sz = jerry_get_string_size(args_p[0].u.v_string);
    int len_name = jerry_string_to_char_buffer(args_p[0].u.v_string, (jerry_char_t *) name, sz);
    name[len_name] = '\0';

    sz = jerry_get_string_size(v_uuid.u.v_string);
    int len_uuid = jerry_string_to_char_buffer(v_uuid.u.v_string, (jerry_char_t *) uuid, sz);
    uuid[len_uuid] = '\0';

    struct bt_data sd[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, name, len_name),
    };

    /*
     * FIXME: fill ad struct with uuid from javascript
     *
     * Set Advertisement data. Based on the Eddystone specification:
     * https://github.com/google/eddystone/blob/master/protocol-specification.md
     * https://github.com/google/eddystone/tree/master/eddystone-url
     */
    struct bt_data ad[] = {
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

    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    PRINT("====>AdvertisingStarted..........\n");
    PRINT("name: %s uuid: %s\n", name, uuid);
    zjs_queue_dispatch("advertisingStart", zjs_bt_adv_start_call_function, err);
    return true;
}

bool zjs_ble_adv_stop(const jerry_object_t *function_obj_p,
                      const jerry_value_t *this_p,
                      jerry_value_t *ret_val_p,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt)
{
    PRINT("stopAdvertising has been called\n");
    return true;
}

bool zjs_ble_parse_characteristic(const jerry_value_t val,
                                  struct ble_characteristic *chrc)
{
    if (val.type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("invalid object type\n");
        return false;
    }

    char uuid[UUID_LEN];
    if (!zjs_obj_get_string(val.u.v_object, "uuid", uuid, UUID_LEN)) {
        PRINT("characteristic uuid doesn't exist\n");
        return false;
    }

    chrc->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_properties;
    if (!jerry_get_object_field_value(val.u.v_object, "properties", &v_properties)) {
        PRINT("properties doesn't exist\n");
        return false;
    }

    int p_index = 0;
    jerry_value_t v_property;
    if (!jerry_get_array_index_value(v_properties.u.v_object, p_index, &v_property)) {
        PRINT("failed to access property array\n");
        return false;
    }

    chrc->flags = 0;
    while (v_property.type == JERRY_DATA_TYPE_STRING) {
        char name[20];
        jerry_size_t sz;
        sz = jerry_get_string_size(v_property.u.v_string);
        int len = jerry_string_to_char_buffer(v_property.u.v_string, (jerry_char_t *)name, sz);
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
        if (!jerry_get_array_index_value(v_properties.u.v_object, p_index, &v_property)) {
            PRINT("failed to access property array\n");
            return false;
        }
    }

    jerry_value_t v_func;
    if (jerry_get_object_field_value(val.u.v_object, "onReadRequest", &v_func)) {
        if (v_func.type != JERRY_DATA_TYPE_NULL) {
            if (v_func.type != JERRY_DATA_TYPE_OBJECT) {
                PRINT("callback is not a function\n");
                return false;
            }
            chrc->read_cb = v_func.u.v_object;
            jerry_acquire_object(v_func.u.v_object);
        }
    }

    if (jerry_get_object_field_value(val.u.v_object, "onWriteRequest", &v_func)) {
        if (v_func.type != JERRY_DATA_TYPE_NULL) {
            if (v_func.type != JERRY_DATA_TYPE_OBJECT) {
                PRINT("callback is not a function\n");
                return false;
            }
            chrc->write_cb = v_func.u.v_object;
            jerry_acquire_object(v_func.u.v_object);
        }
    }

    if (jerry_get_object_field_value(val.u.v_object, "onSubscribe", &v_func)) {
        if (v_func.type != JERRY_DATA_TYPE_NULL) {
            if (v_func.type != JERRY_DATA_TYPE_OBJECT) {
                PRINT("callback is not a function\n");
                return false;
            }
            chrc->subscribe_cb = v_func.u.v_object;
            jerry_acquire_object(v_func.u.v_object);
        }
    }

    if (jerry_get_object_field_value(val.u.v_object, "onUnsubscribe", &v_func)) {
        if (v_func.type != JERRY_DATA_TYPE_NULL) {
            if (v_func.type != JERRY_DATA_TYPE_OBJECT) {
                PRINT("callback is not a function\n");
                return false;
            }
            chrc->unsubscribe_cb = v_func.u.v_object;
            jerry_acquire_object(v_func.u.v_object);
        }
    }

    if (jerry_get_object_field_value(val.u.v_object, "onNotify", &v_func)) {
        if (v_func.type != JERRY_DATA_TYPE_NULL) {
            if (v_func.type != JERRY_DATA_TYPE_OBJECT) {
                PRINT("callback is not a function\n");
                return false;
            }
            chrc->notify_cb = v_func.u.v_object;
            jerry_acquire_object(v_func.u.v_object);
        }
    }

    return true;
}

bool zjs_ble_parse_service(const jerry_value_t val,
                           struct ble_service *service)
{
    if (!service) {
        return false;
    }

    if (val.type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("invalid object type\n");
        return false;
    }

    char uuid[UUID_LEN];
    if (!zjs_obj_get_string(val.u.v_object, "uuid", uuid, UUID_LEN)) {
        PRINT("service uuid doesn't exist\n");
        return false;
    }
    service->uuid = zjs_ble_new_uuid_16(strtoul(uuid, NULL, 16));

    jerry_value_t v_array;
    if (!jerry_get_object_field_value(val.u.v_object, "characteristics", &v_array)) {
        PRINT("characteristics doesn't exist\n");
        return false;
    }

    if (v_array.type != JERRY_DATA_TYPE_OBJECT) {
        PRINT("characteristics array is undefined or null\n");
        return false;
    }

    int chrc_index = 0;
    jerry_value_t v_characteristic;
    if (!jerry_get_array_index_value(v_array.u.v_object, chrc_index, &v_characteristic)) {
        PRINT("failed to access characteristic array\n");
        return false;
    }

    struct ble_characteristic *previous = NULL;
    while (v_characteristic.type == JERRY_DATA_TYPE_OBJECT) {
        struct ble_characteristic *chrc = task_malloc(sizeof(struct ble_characteristic));
        if (!chrc) {
            PRINT("error: out of memory allocating struct ble_characteristic\n");
        } else {
            memset(chrc, 0, sizeof(struct ble_characteristic));
        }

        if (!zjs_ble_parse_characteristic(v_characteristic, chrc)) {
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
        if (!jerry_get_array_index_value(v_array.u.v_object, chrc_index, &v_characteristic)) {
            PRINT("failed to access characteristic array\n");
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

bool zjs_ble_register_service(struct ble_service *service)
{
    if (!service) {
        PRINT("zjs_ble_create_gatt_attrs: invalid ble_service\n");
    }

    int num_of_entries = 1; // 1 attribute for service uuid
    struct ble_characteristic *ch = service->characteristics;
    while (ch != NULL) {
        num_of_entries += 4;  // 4 attributes per characteristic
        ch = ch->next;
    }

   struct bt_gatt_attr* bt_attrs = task_malloc(sizeof(struct bt_gatt_attr) * num_of_entries);
    if (!bt_attrs) {
        PRINT("error: out of memory allocating struct bt_gatt_attr\n");
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
        if (ch->read_cb) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_READ;
        }
        if (ch->write_cb) {
            bt_attrs[entry_index].perm |= BT_GATT_PERM_WRITE;
        }
        bt_attrs[entry_index].read = read_callback;
        bt_attrs[entry_index].write = write_callback;
        bt_attrs[entry_index].user_data = ch;

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

            ccc_user_data->cfg = blvl_ccc_cfg;
            ccc_user_data->cfg_len = ARRAY_SIZE(blvl_ccc_cfg);
            ccc_user_data->cfg_changed = blvl_ccc_cfg_changed;
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

bool zjs_ble_set_services(const jerry_object_t *function_obj_p,
                          const jerry_value_t *this_p,
                          jerry_value_t *ret_val_p,
                          const jerry_value_t args_p[],
                          const jerry_length_t args_cnt)
{
    PRINT("setServices has been called\n");

    if (args_cnt != 1 ||
        args_p[0].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_set_services: invalid arguments\n");
        return false;
    }

    if (args_cnt == 2 &&
        args_p[1].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_set_services: invalid arguments\n");
        return false;
    }

    // FIXME: currently hard-coded to work with demo
    // which has only 1 primary service and 2 characterstics
    // add support for multiple services
    jerry_value_t v_service;
    if (!jerry_get_array_index_value(args_p[0].u.v_object, 0, &v_service)) {
        PRINT("zjs_ble_set_services: services array is empty\n");
        return false;
    }

    if (zjs_primary_service.characteristics) {
        zjs_ble_free_characteristics(zjs_primary_service.characteristics);
    }

    if (!zjs_ble_parse_service(v_service, &zjs_primary_service)) {
        PRINT("zjs_ble_set_services: failed to validate service object\n");
        return false;
    }

    return zjs_ble_register_service(&zjs_primary_service);
}

bool zjs_ble_primary_service(const jerry_object_t *function_obj_p,
                             const jerry_value_t *this_p,
                             jerry_value_t *ret_val_p,
                             const jerry_value_t args_p[],
                             const jerry_length_t args_cnt)
{
    PRINT("new PrimaryService has been called\n");

    if (args_cnt < 1 ||
        args_p[0].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_primary_service: invalid arguments\n");
        return false;
    }

    jerry_acquire_object(args_p[0].u.v_object);
    *ret_val_p = args_p[0];

    return true;
}

bool zjs_ble_characteristic(const jerry_object_t *function_obj_p,
                            const jerry_value_t *this_p,
                            jerry_value_t *ret_val_p,
                            const jerry_value_t args_p[],
                            const jerry_length_t args_cnt)
{
    PRINT("new Characterstic has been called\n");

    if (args_cnt < 1 ||
        args_p[0].type != JERRY_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_ble_characterstic: invalid arguments\n");
        return false;
    }

    jerry_acquire_object(args_p[0].u.v_object);
    *ret_val_p = args_p[0];

    return true;
}
