// Copyright (c) 2017, Intel Corporation.

#ifndef ZJS_LINUX_BUILD
#include <bluetooth/storage.h>
#endif

#include "zjs_util.h"

void zjs_net_config(void);

/*
 * API to initialize/set the BLE address. This is called automatically by
 * zjs_net_config(), but since OCF internally starts BLE, this must be left
 * available for it to call manually.
 */
void zjs_init_ble_address();

/*
 * JS native API provided to set the BLE address from JavaScript
 */
ZJS_DECL_FUNC(zjs_set_ble_address);
