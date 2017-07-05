// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_ocf_client__
#define __zjs_ocf_client__

#include "jerryscript.h"

/*
 * Initialize the OCF client object
 */
jerry_value_t zjs_ocf_client_init();

/*
 * Cleanup OCF client
 */
void zjs_ocf_client_cleanup();

#endif  // __zjs_ocf_client__
