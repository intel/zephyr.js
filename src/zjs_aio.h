// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_aio_h__
#define __zjs_aio_h__

#include "jerry-api.h"

jerry_object_t *zjs_aio_init();

bool zjs_aio_open(const jerry_object_t *function_obj_p,
                  const jerry_value_t this_val,
                  const jerry_value_t args_p[],
                  const jerry_length_t args_cnt,
                  jerry_value_t *ret_val_p);

bool zjs_aio_pin_read(const jerry_object_t *function_obj_p,
                      const jerry_value_t this_val,
                      const jerry_value_t args_p[],
                      const jerry_length_t args_cnt,
                      jerry_value_t *ret_val_p);

bool zjs_aio_pin_abort(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p);

bool zjs_aio_pin_close(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p);

bool zjs_aio_pin_read_async(const jerry_object_t *function_obj_p,
                            const jerry_value_t this_val,
                            const jerry_value_t args_p[],
                            const jerry_length_t args_cnt,
                            jerry_value_t *ret_val_p);

bool zjs_aio_pin_on(const jerry_object_t *function_obj_p,
                    const jerry_value_t this_val,
                    const jerry_value_t args_p[],
                    const jerry_length_t args_cnt,
                    jerry_value_t *ret_val_p);

#endif  // __zjs_aio_h__
