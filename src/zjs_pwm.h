// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_pwm_h__
#define __zjs_pwm_h__

#include "jerry-api.h"

extern void (*zjs_pwm_convert_pin)(uint32_t num, int *dev, int *pin);

jerry_value_t zjs_pwm_init();

#endif  // __zjs_pwm_h__
