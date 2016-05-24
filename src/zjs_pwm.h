// Copyright (c) 2016, Intel Corporation.

#include "jerry-api.h"

jerry_api_object_t *zjs_pwm_init();

bool zjs_pwm_open(const jerry_api_object_t *function_obj_p,
                  const jerry_api_value_t *this_p,
                  jerry_api_value_t *ret_val_p,
                  const jerry_api_value_t args_p[],
                  const jerry_api_length_t args_cnt);

bool zjs_pwm_pin_set_period(const jerry_api_object_t *function_obj_p,
                            const jerry_api_value_t *this_p,
                            jerry_api_value_t *ret_val_p,
                            const jerry_api_value_t args_p[],
                            const jerry_api_length_t args_cnt);

bool zjs_pwm_pin_set_duty_cycle(const jerry_api_object_t *function_obj_p,
                                const jerry_api_value_t *this_p,
                                jerry_api_value_t *ret_val_p,
                                const jerry_api_value_t args_p[],
                                const jerry_api_length_t args_cnt);

void zjs_pwm_set(uint32_t channel, uint32_t period, uint32_t dutyCycle,
                 const char *polarity);
