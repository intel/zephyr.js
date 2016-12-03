// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_aio_h__
#define __zjs_aio_h__

#include "jerry-api.h"

/** Initialize the aio module, or reinitialize after cleanup */
jerry_value_t zjs_aio_init();

/** Release resources held by the aio module */
void zjs_aio_cleanup();

#endif  // __zjs_aio_h__
