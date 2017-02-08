// Copyright (c) 2016-2017, Intel Corporation.

#include "zjs_ocf_ble.h"

static bt_addr_le_t id_addr;

static ssize_t zjs_ble_storage_read(const bt_addr_le_t *addr, uint16_t key,
    void *data, size_t length)
{
    if (addr) {
        return -ENOENT;
    }

    if (key == BT_STORAGE_ID_ADDR && length == sizeof(id_addr) &&
        bt_addr_le_cmp(&id_addr, BT_ADDR_LE_ANY)) {
        bt_addr_le_copy(data, &id_addr);
        return sizeof(id_addr);
    }

    return -EIO;
}

/*
 * Convert a device ID (MAC) string to Zephyr bluetooth address
 * e.g. input: "AA:BB:CC:DD:EE:FF"
 */
static int str2bt_addr_le(const char *str, const char *type, bt_addr_le_t *addr)
{
    int i;
    uint8_t tmp;

    if (strlen(str) != 17) {
        return -EINVAL;
    }

    for (i = 5; i >= 0; str+=3, i--) {
        if (!zjs_hex_to_byte(str, &tmp)) {
            return -EINVAL;
        }
        addr->a.val[i] = tmp;
    }

    if (!strcmp(type, "public") || !strcmp(type, "(public)")) {
        addr->type = BT_ADDR_LE_PUBLIC;
    } else if (!strcmp(type, "random") || !strcmp(type, "(random)")) {
        addr->type = BT_ADDR_LE_RANDOM;
    } else {
        return -EINVAL;
    }

    return 0;
}

static jerry_value_t ocf_set_ble_address(const jerry_value_t function_val,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc)
{
    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("invalid arguments");
    }
    jerry_size_t size = 18;
    char addr[size];
    zjs_copy_jstring(argv[0], addr, &size);

    static const struct bt_storage storage = {
            .read = zjs_ble_storage_read,
            .write = NULL,
            .clear = NULL,
    };

    if (str2bt_addr_le(addr, "random", &id_addr) < 0) {
        return zjs_error("bad BLE address string");
    }
    DBG_PRINT("BLE addr is set to: %s\n", addr);
    BT_ADDR_SET_STATIC(&id_addr.a);
    bt_storage_register(&storage);

    return ZJS_UNDEFINED;
}

void zjs_init_ocf_ble()
{
    jerry_value_t global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, ocf_set_ble_address, "setBleAddress");
    jerry_release_value(global_obj);
}
