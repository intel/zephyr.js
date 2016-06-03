// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

jerry_api_object_t *zjs_aio_init();

bool zjs_aio_open(const jerry_api_object_t *function_obj_p,
                  const jerry_api_value_t *this_p,
                  jerry_api_value_t *ret_val_p,
                  const jerry_api_value_t args_p[],
                  const jerry_api_length_t args_cnt);

bool zjs_aio_pin_read(const jerry_api_object_t *function_obj_p,
                      const jerry_api_value_t *this_p,
                      jerry_api_value_t *ret_val_p,
                      const jerry_api_value_t args_p[],
                      const jerry_api_length_t args_cnt);

bool zjs_aio_pin_abort(const jerry_api_object_t *function_obj_p,
                       const jerry_api_value_t *this_p,
                       jerry_api_value_t *ret_val_p,
                       const jerry_api_value_t args_p[],
                       const jerry_api_length_t args_cnt);

bool zjs_aio_pin_close(const jerry_api_object_t *function_obj_p,
                       const jerry_api_value_t *this_p,
                       jerry_api_value_t *ret_val_p,
                       const jerry_api_value_t args_p[],
                       const jerry_api_length_t args_cnt);

bool zjs_aio_pin_read_async(const jerry_api_object_t *function_obj_p,
                            const jerry_api_value_t *this_p,
                            jerry_api_value_t *ret_val_p,
                            const jerry_api_value_t args_p[],
                            const jerry_api_length_t args_cnt);
