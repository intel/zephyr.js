// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_webusb_h__
#define __zjs_webusb_h__

#include "jerryscript.h"

/**
 * Initialize the webusb module, or reinitialize after cleanup
 *
 * @return WebUSB API object
 */
jerry_value_t zjs_webusb_init();

/** Release resources held by the webusb module */
void zjs_webusb_cleanup();

#endif  // __zjs_webusb_h__
