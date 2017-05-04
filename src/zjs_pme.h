// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_pme_h__
#define __zjs_pme_h__

#include "jerryscript.h"

/**
 * Initialize the pme module, or reinitialize after cleanup
 *
 * @return PME API object
 */
jerry_value_t zjs_pme_init();

/** Release resources held by the pme module */
void zjs_pme_cleanup();

#endif // __zjs_pme_h__
