// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_gpio_h__
#define __zjs_gpio_h__

#include "jerry-api.h"

extern void (*zjs_gpio_convert_pin)(uint32_t orig, int *dev, int *pin);

/**
 * Initialize the gpio module, or reinitialize after cleanup
 *
 * @return GPIO API object
 */
jerry_value_t zjs_gpio_init();

/** Release resources held by the gpio module */
void zjs_gpio_cleanup();

#endif  // __zjs_gpio_h__
