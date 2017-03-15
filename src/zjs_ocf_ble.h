// Copyright (c) 2016-2017, Intel Corporation.

#ifndef SRC_ZJS_OCF_BLE_H_
#define SRC_ZJS_OCF_BLE_H_

#include <bluetooth/storage.h>

#include "zjs_util.h"

void zjs_init_ocf_ble();

jerry_value_t zjs_ocf_set_ble_address(const jerry_value_t function_val,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc);

#endif /* SRC_ZJS_OCF_BLE_H_ */
