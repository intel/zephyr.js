// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_gpio_h__
#define __zjs_gpio_h__

#include "jerryscript.h"

/**
 * Initialize the gpio module, or reinitialize after cleanup
 *
 * @return GPIO API object
 */
jerry_value_t zjs_gpio_init();

/** Release resources held by the gpio module */
void zjs_gpio_cleanup();

#endif  // __zjs_gpio_h__
