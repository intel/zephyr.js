// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_ble_h__
#define __zjs_ble_h__

#include "jerry-api.h"

/**
 * Initialize the ble module, or reinitialize after cleanup
 *
 * @return BLE API object
 */
jerry_value_t zjs_ble_init();

/** Release resources held by the ble module */
void zjs_ble_cleanup();

void zjs_ble_enable();

#endif  // __zjs_ble_h__
