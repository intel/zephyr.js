// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_gpio_h__
#define __zjs_gpio_h__

#include "jerry-api.h"

extern int (*zjs_gpio_convert_pin)(int num);

jerry_value_t zjs_gpio_init();

#endif  // __zjs_gpio_h__