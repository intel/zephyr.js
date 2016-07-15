// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_gpio_h__
#define __zjs_gpio_h__

#include "jerry-api.h"

extern int (*zjs_gpio_convert_pin)(int num);

jerry_object_t *zjs_gpio_init();

bool zjs_gpio_open(const jerry_object_t *function_obj_p,
                   const jerry_value_t this_val,
                   const jerry_value_t args_p[],
                   const jerry_length_t args_cnt,
                   jerry_value_t *ret_val_p);

bool zjs_gpio_pin_read(const jerry_object_t *function_obj_p,
                       const jerry_value_t this_val,
                       const jerry_value_t args_p[],
                       const jerry_length_t args_cnt,
                       jerry_value_t *ret_val_p);

bool zjs_gpio_pin_write(const jerry_object_t *function_obj_p,
                        const jerry_value_t this_val,
                        const jerry_value_t args_p[],
                        const jerry_length_t args_cnt,
                        jerry_value_t *ret_val_p);

bool zjs_gpio_pin_on(const jerry_object_t *function_obj_p,
                     const jerry_value_t this_val,
                     const jerry_value_t args_p[],
                     const jerry_length_t args_cnt,
                     jerry_value_t *ret_val_p);

#endif  // __zjs_gpio_h__
