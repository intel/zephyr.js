// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_ble_h__
#define __zjs_ble_h__

#include "jerry-api.h"

jerry_object_t *zjs_ble_init();

void zjs_ble_enable();

bool zjs_ble_on(const jerry_object_t *function_obj_p,
                const jerry_value_t this_val,
                const jerry_value_t args_p[],
                const jerry_length_t args_cnt,
                jerry_value_t *ret_val_p);

bool zjs_ble_adv_start(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p);

bool zjs_ble_adv_stop(const jerry_object_t *function_obj_p,
                      const jerry_value_t this_val,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt,
                      jerry_value_t *ret_val_p);

bool zjs_ble_set_services(const jerry_object_t *function_obj_p,
                          const jerry_value_t this_val,
                          const jerry_value_t args_p[],
                          const jerry_length_t args_cnt,
                          jerry_value_t *ret_val_p);

bool zjs_ble_primary_service(const jerry_object_t *function_obj_p,
                             const jerry_value_t this_val,
                             const jerry_value_t args_p[],
                             const jerry_length_t args_cnt,
                             jerry_value_t *ret_val_p);

bool zjs_ble_characteristic(const jerry_object_t *function_obj_p,
                            const jerry_value_t this_val,
                            const jerry_value_t args_p[],
                            const jerry_length_t args_cnt,
                            jerry_value_t *ret_val_p);

#endif  // __zjs_ble_h__
