// Copyright (c) 2016-2017, Intel Corporation.

#include <net/net_context.h>
#include <net/nbuf.h>
#include <net/net_if.h>
#if defined(CONFIG_NET_L2_BLUETOOTH)
#include <bluetooth/bluetooth.h>
#include <gatt/ipss.h>
#endif
#include "zjs_util.h"
#include "zjs_net_config.h"

static uint8_t net_enabled = 0;
#if defined(CONFIG_NET_L2_BLUETOOTH)
static uint8_t ble_enabled = 0;
#endif

void zjs_net_config(void)
{
#if defined(CONFIG_NET_L2_BLUETOOTH)
    if (!ble_enabled) {
        zjs_init_ble_address();
        if (bt_enable(NULL)) {
            ERR_PRINT("Bluetooth init failed");
            return;
        }
        ipss_init();
        ipss_advertise();
        ble_enabled = 1;
    }
#endif
    if (!net_enabled) {
        // TODO: Provide way to configure IP address/DHCP if desired
#ifdef CONFIG_NET_IPV4
        static struct in_addr in4addr_my = { { { 192, 0, 2, 1 } } };
        net_if_ipv4_addr_add(net_if_get_default(), &in4addr_my, NET_ADDR_MANUAL, 0);
#endif
#ifdef CONFIG_NET_IPV6
        // 2001:db8::1
        static struct in6_addr in6addr_my = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                                                  0, 0, 0, 0, 0, 0, 0, 0x1 } } };
        net_if_ipv6_addr_add(net_if_get_default(), &in6addr_my, NET_ADDR_MANUAL, 0);
#endif
        net_enabled = 1;
    }
}

static bt_addr_le_t id_addr;
#ifdef ZJS_CONFIG_BLE_ADDRESS
static char default_ble[18] = ZJS_CONFIG_BLE_ADDRESS;
#endif

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

    if (strnlen(str, 18) != 17) {
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

jerry_value_t zjs_set_ble_address(const jerry_value_t function_val,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
#ifndef ZJS_CONFIG_BLE_ADDRESS
    // args: address
    ZJS_VALIDATE_ARGS(Z_STRING);

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
#endif
    return ZJS_UNDEFINED;
}

void zjs_init_ble_address()
{
#ifdef ZJS_CONFIG_BLE_ADDRESS
    static const struct bt_storage storage = {
            .read = zjs_ble_storage_read,
            .write = NULL,
            .clear = NULL,
    };

    if (str2bt_addr_le(default_ble, "random", &id_addr) < 0) {
        ERR_PRINT("bad BLE address string");
        return;
    }
    DBG_PRINT("BLE addr is set to: %s\n", default_ble);
    BT_ADDR_SET_STATIC(&id_addr.a);
    bt_storage_register(&storage);
#endif
}
