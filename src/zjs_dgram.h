// Copyright (c) 2017, Linaro Limited.

#ifndef __zjs_dgram_h__
#define __zjs_dgram_h__

#include "jerry-api.h"

/** Initialize the dgram module, or reinitialize after cleanup */
jerry_value_t zjs_dgram_init();

/** Release resources held by the dgram module */
void zjs_dgram_cleanup();

#endif  // __zjs_dgram_h__
