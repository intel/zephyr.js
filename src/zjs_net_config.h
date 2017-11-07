// Copyright (c) 2017, Intel Corporation.

#include "zjs_util.h"

#ifndef ZJS_LINUX_BUILD
#include <net/net_context.h>

#ifdef CONFIG_NET_L2_BT
extern u8_t net_ble_enabled;
#endif
#endif

/*
 * Do the default network configuration. This should always be called when
 * initializing a networking module. If the 'net_config' require was used,
 * this function has no effect.
 */
void zjs_net_config_default(void);

/*
 * API to initialize/set the BLE address. This is called automatically by
 * zjs_net_config(), but since OCF internally starts BLE, this must be left
 * available for it to call manually.
 */
void zjs_init_ble_address();

#ifndef ZJS_LINUX_BUILD
/*
 * Get the local IP address for a net_context *. This will return either an
 * IPv4 or IPv6 address depending on how the net_context was configured.
 */
struct sockaddr *zjs_net_config_get_ip(struct net_context *context);

/*
 * Get the IP version of an IP address string
 */
int zjs_is_ip(char *addr);
#endif
