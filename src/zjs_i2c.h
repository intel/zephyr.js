// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_i2c_h__
#define __zjs_i2c_h__

#include "jerry-api.h"

/**
 * Initialize the i2c module, or reinitialize after cleanup
 *
 * @return I2C API object
 */
jerry_value_t zjs_i2c_init();

/** Release resources held by the i2c module */
void zjs_i2c_cleanup();

#endif  // __zjs_i2c_h__
