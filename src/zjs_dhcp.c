// Copyright (c) 2017, Intel Corporation.

#include <zephyr.h>
#include <sections.h>
#include <errno.h>
#include <stdio.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

static struct net_mgmt_event_callback mgmt_cb;
static zjs_callback_id id = -1;

static void callback(struct net_mgmt_event_callback *cb,
                     u32_t mgmt_event,
                     struct net_if *iface)
{
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
        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 3);
        id = -1;

        return;
    }
}

static ZJS_DECL_FUNC(dhcp_start)
{
    struct net_if *iface;

    net_mgmt_init_event_callback(&mgmt_cb, callback,
            NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgmt_cb);

    iface = net_if_get_default();

    id = zjs_add_callback_once(argv[0], this, NULL, NULL);

    net_dhcpv4_start(iface);

    return ZJS_UNDEFINED;
}

jerry_value_t zjs_dhcp_init()
{
    jerry_value_t dhcp = jerry_create_object();

    zjs_obj_add_function(dhcp, dhcp_start, "start");

    return dhcp;
}

void zjs_dhcp_cleanup()
{
    if (id !=- 1) {
        zjs_remove_callback(id);
        id = -1;
    }
}
