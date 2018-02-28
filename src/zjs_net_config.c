// Copyright (c) 2016-2018, Intel Corporation.

// enable to use function tracing for debug purposes
#if 0
#define USE_FTRACE
static char FTRACE_PREFIX[] = "net";
#endif

#include <net/net_if.h>
#ifdef BUILD_MODULE_NET_CONFIG
#include <net/net_context.h>
#include <net/net_mgmt.h>
#endif

#ifdef CONFIG_NET_L2_BT
#include <bluetooth/bluetooth.h>
#include <bluetooth/storage.h>
#include <gatt/ipss.h>
#endif

#include "zjs_callbacks.h"
#include "zjs_event.h"
#include "zjs_net_config.h"
#include "zjs_util.h"

#ifndef BUILD_MODULE_NET_CONFIG
static u8_t net_enabled = 0;
#endif
#ifdef CONFIG_NET_L2_BT
u8_t net_ble_enabled = 0;
#endif

void zjs_net_config_default(void)
{
    /*
     * if netconfig was not included, just do the default configuration
     */
    FTRACE("\n");
#ifndef BUILD_MODULE_NET_CONFIG
#ifdef CONFIG_NET_L2_BT
    if (!net_ble_enabled) {
        zjs_init_ble_address();
    }
#endif
    if (!net_enabled) {
        // TODO: Provide way to configure IP address/DHCP if desired
#ifdef CONFIG_NET_IPV4
        static struct in_addr in4addr_my = { { { 192, 0, 2, 1 } } };
        net_if_ipv4_addr_add(net_if_get_default(), &in4addr_my, NET_ADDR_MANUAL,
                             0);
#endif
#ifdef CONFIG_NET_IPV6
        // 2001:db8::1
        static struct in6_addr in6addr_my = { { { 0x20, 0x01, 0x0d, 0xb8,
                                                  0, 0, 0, 0, 0, 0, 0, 0,
                                                  0, 0, 0, 0x1 } } };
        net_if_ipv6_addr_add(net_if_get_default(), &in6addr_my, NET_ADDR_MANUAL, 0);
#endif
        net_enabled = 1;
    }
#endif
}

struct sockaddr *zjs_net_config_get_ip(struct net_context *context)
{
    // effects: returns pointer to sockaddr for a valid, non-link-local address
    //            of the right family for this context, or NULL if not found
    FTRACE("context = %p\n", context);
    struct net_if *iface = net_context_get_iface(context);

    if (net_context_get_family(context) == AF_INET) {
#ifdef CONFIG_NET_IPV4
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            if (iface->ipv4.unicast[i].is_used) {
                return (struct sockaddr *)&iface->ipv4.unicast[i].address;
            }
        }
#endif
    } else {
#ifdef CONFIG_NET_IPV6
        for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
            if (iface->ipv6.unicast[i].is_used) {
                struct net_addr *addr = &iface->ipv6.unicast[i].address;
                struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
                if (in6->sin6_addr.in6_u.u6_addr8[0] != 0xfe ||
                    (in6->sin6_addr.in6_u.u6_addr8[1] & 0xc0) != 0x80) {
                    // not link local, use this one
                    return (struct sockaddr *)addr;
                }
            }
        }
#endif
    }
    return NULL;
}

int zjs_is_ip(char *addr)
{
    FTRACE("addr = '%s'\n", addr);
    struct sockaddr_in6 tmp = { 0 };

    // check if v6
    if (net_addr_pton(AF_INET6, addr, &tmp.sin6_addr) < 0) {
        // check if v4
        struct sockaddr_in tmp1 = { 0 };
        if (net_addr_pton(AF_INET, addr, &tmp1.sin_addr) < 0) {
            return -1;
        } else {
            return 4;
        }
    } else {
        return 6;
    }
}

#ifdef CONFIG_NET_L2_BT
static bt_addr_le_t id_addr;
#ifdef ZJS_CONFIG_BLE_ADDRESS
static char default_ble[18] = ZJS_CONFIG_BLE_ADDRESS;
#else
static char default_ble[18] = "FF:EE:DD:CC:BB:AA";
#endif

static ssize_t zjs_ble_storage_read(const bt_addr_le_t *addr, u16_t key,
                                    void *data, size_t length)
{
    FTRACE("addr = %p, key = %d, data = %p, length = %d\n", addr, (u32_t)key,
           data, (u32_t)length);
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
    FTRACE("str = '%s', type = '%s', addr = %p\n", str, type, addr);
    int i;
    u8_t tmp;

    if (strnlen(str, 18) != 17) {
        return -EINVAL;
    }

    for (i = 5; i >= 0; str += 3, i--) {
        if (!zjs_hex_to_byte(str, &tmp)) {
            return -EINVAL;
        }
        addr->a.val[i] = tmp;
    }

    if (strequal(type, "public") || strequal(type, "(public)")) {
        addr->type = BT_ADDR_LE_PUBLIC;
    } else if (strequal(type, "random") || strequal(type, "(random)")) {
        addr->type = BT_ADDR_LE_RANDOM;
    } else {
        return -EINVAL;
    }

    return 0;
}

static void ble_set_address(char *ble_addr)
{
    FTRACE("ble_addr = %s\n", ble_addr);
    static const struct bt_storage storage = {
        .read = zjs_ble_storage_read,
        .write = NULL,
        .clear = NULL,
    };

    if (str2bt_addr_le(ble_addr, "random", &id_addr) < 0) {
        ERR_PRINT("bad BLE address string\n");
        return;
    }

    // set the top two bits since Zephyr always sets them anyway, so we will
    //   print the actual value the user will see. For example,
    //   11:22:33:44:55:66 will change to D1:22:33:44:55:66.
    // TODO: find out why this happens and whether it must; if so, warn the
    //   user at compile-time instead of just here
    char hex[3];
    hex[0] = ble_addr[0];
    hex[1] = '0';
    hex[2] = '\0';
    u8_t byte;
    zjs_hex_to_byte(hex, &byte);
    u8_t newbyte = byte | 0xC0;
    if (byte != newbyte) {
        ZJS_PRINT("Warning: Requested BLE address %s had to be modified!\n",
                  ble_addr);
    }
    default_ble[0] = 'C' + (newbyte >> 4) - 12;

    DBG_PRINT("set BLE address to: %s\n", default_ble);
    BT_ADDR_SET_STATIC(&id_addr.a);
    bt_storage_register(&storage);
}

void zjs_init_ble_address()
{
    FTRACE("\n");
    ble_set_address(default_ble);
}
#endif

#ifdef BUILD_MODULE_NET_CONFIG
#ifdef CONFIG_NET_DHCPV4
static struct net_mgmt_event_callback dhcp_cb;
static zjs_callback_id dhcp_id = -1;

static void dhcp_callback(struct net_mgmt_event_callback *cb,
                          u32_t mgmt_event,
                          struct net_if *iface)
{
    FTRACE("cb = %p, mgmt_event = %x, iface = %p\n", cb, mgmt_event, iface);
    int i = 0;

    if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
        return;
    }

    for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        char buf[NET_IPV4_ADDR_LEN];

        if (iface->ipv4.unicast[i].addr_type != NET_ADDR_DHCP) {
            continue;
        }

        net_addr_ntop(AF_INET,
                      &iface->ipv4.unicast[i].address.in_addr,
                      buf,
                      sizeof(buf));
        ZVAL addr = jerry_create_string(buf);

        // NET_INFO("Lease time: %u seconds", iface->dhcpv4.lease_time);
        net_addr_ntop(AF_INET,
                      &iface->ipv4.netmask,
                      buf,
                      sizeof(buf));
        ZVAL subnet = jerry_create_string(buf);

        net_addr_ntop(AF_INET,
                      &iface->ipv4.gw,
                      buf,
                      sizeof(buf));
        ZVAL gateway = jerry_create_string(buf);

        jerry_value_t args[] = { addr, subnet, gateway };
        zjs_signal_callback(dhcp_id, args, sizeof(args));
        dhcp_id = -1;

        return;
    }
}

static ZJS_DECL_FUNC(dhcp)
{
    ZJS_VALIDATE_ARGS(Z_FUNCTION);

    struct net_if *iface;

    net_mgmt_init_event_callback(&dhcp_cb, dhcp_callback,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&dhcp_cb);

    iface = net_if_get_default();

    dhcp_id = zjs_add_callback_once(argv[0], this, NULL, NULL);

    net_dhcpv4_start(iface);

    return ZJS_UNDEFINED;
}
#endif

#ifdef CONFIG_NET_L2_BT
static ZJS_DECL_FUNC(set_ble_address)
{
#ifndef ZJS_CONFIG_BLE_ADDRESS
    // args: address
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = 18;
    char addr[size];
    zjs_copy_jstring(argv[0], addr, &size);
    ble_set_address(addr);
#endif
    return ZJS_UNDEFINED;
}
#endif

static ZJS_DECL_FUNC(set_ip)
{
    ZJS_VALIDATE_ARGS(Z_STRING);

    uint32_t size = 64;
    char str[size];

    zjs_copy_jstring(argv[0], str, &size);
    if (!size) {
        return RANGE_ERROR("IP address is too long");
    }

    struct sockaddr_in6 tmp = { 0 };

    // check if v6
    if (net_addr_pton(AF_INET6, str, &tmp.sin6_addr) < 0) {
        // check if v4
        struct sockaddr_in tmp1 = { 0 };
        if (net_addr_pton(AF_INET, str, &tmp1.sin_addr) < 0) {
            return TYPE_ERROR("String was not an IP address");
        } else {
#ifndef CONFIG_NET_IPV4
            return NOTSUPPORTED_ERROR("IPv4 not supported");
#else
            if (!net_if_ipv4_addr_add(net_if_get_default(), &tmp1.sin_addr,
                                      NET_ADDR_MANUAL, 0)) {
                return jerry_create_boolean(false);
            }
#endif
        }
    } else {
#ifndef CONFIG_NET_IPV6
        return NOTSUPPORTED_ERROR("IPv6 not supported");
#else
        if (!net_if_ipv6_addr_add(net_if_get_default(), &tmp.sin6_addr,
                                  NET_ADDR_MANUAL, 0)) {
            return jerry_create_boolean(false);
        }
#endif
    }

    return jerry_create_boolean(true);
}

static jerry_value_t config;
static struct net_mgmt_event_callback cb;

static void iface_event(struct net_mgmt_event_callback *cb,
                        u32_t mgmt_event, struct net_if *iface)
{
    FTRACE("cb = %p, mgmt_event = %x, iface = %p\n", cb, mgmt_event, iface);
    if (mgmt_event == NET_EVENT_IF_UP) {
        zjs_defer_emit_event(config, "netup", NULL, 0, NULL, NULL);
    } else if (mgmt_event == NET_EVENT_IF_DOWN) {
        zjs_defer_emit_event(config, "netdown", NULL, 0, NULL, NULL);
    }
}

static jerry_value_t zjs_net_config_init(void)
{
    FTRACE("\n");
    config = zjs_create_object();

    zjs_obj_add_function(config, "setStaticIP", set_ip);
#ifdef CONFIG_NET_DHCPV4
    zjs_obj_add_function(config, "dhcp", dhcp);
#endif
#ifdef CONFIG_NET_L2_BT
    zjs_obj_add_function(config, "setBleAddress", set_ble_address);
#endif
    zjs_make_emitter(config, ZJS_UNDEFINED, NULL, NULL);

    struct net_if *iface = net_if_get_default();
    if (atomic_test_bit(iface->flags, NET_IF_UP)) {
        // networking is already up
        zjs_defer_emit_event(config, "netup", NULL, 0, NULL, NULL);
    }
    // notify when networking goes up/down
    net_mgmt_init_event_callback(&cb, iface_event,
                                 NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
    net_mgmt_add_event_callback(&cb);

#ifdef CONFIG_NET_L2_BT
    if (!net_ble_enabled) {
        zjs_init_ble_address();
    }
#endif

    return config;
}

JERRYX_NATIVE_MODULE(netconfig, zjs_net_config_init)
#endif
