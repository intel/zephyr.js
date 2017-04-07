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
jerry_value_t zjs_set_ble_address(const jerry_value_t function_val,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc);
