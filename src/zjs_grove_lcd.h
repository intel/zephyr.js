// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_grove_lcd_h__
#define __zjs_grove_lcd_h__

#include "jerry-api.h"

jerry_value_t zjs_grove_lcd_init();

/**
 * Initialize the grove_lcd module, or reinitialize after cleanup
 *
 * @return Grove LCD API object
 */
jerry_value_t zjs_grove_lcd_init();

/** Release resources held by the grove_lcd module */
void zjs_grove_lcd_cleanup();

#endif  // __zjs_grove_lcd_h__
