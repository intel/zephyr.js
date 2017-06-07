// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_spi_h__
#define __zjs_spi_h__

#include "jerryscript.h"

/**
 * Initialize the spi module, or reinitialize after cleanup
 *
 * @return SPI API object
 */
jerry_value_t zjs_spi_init();

/** Release resources held by the spi module */
void zjs_spi_cleanup();

#endif  // __zjs_spi_h__
