// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_pwm_h__
#define __zjs_pwm_h__

#include "jerry-api.h"

extern int (*zjs_pwm_convert_pin)(int num);

jerry_object_t *zjs_pwm_init();

#endif  // __zjs_pwm_h__
