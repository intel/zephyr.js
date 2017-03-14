// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_net_h__
#define __zjs_net_h__

#include "jerryscript.h"

/** Initialize the net module, or reinitialize after cleanup */
jerry_value_t zjs_net_init();

/** Release resources held by the net module */
void zjs_net_cleanup();

#endif  // __zjs_net_h__
