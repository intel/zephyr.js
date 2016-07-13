// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

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
