// Copyright (c) 2016, Intel Corporation.

#include "zjs_util.h"
#include "zjs_common.h"

/*
 * Initialize and return OCF server object
 */
jerry_value_t zjs_ocf_server_init();

/*
 * iotivity-constrained callback to register resources.
 */
void zjs_ocf_register_resources(void);

/*
 * OCF server module cleanup
 */
void zjs_ocf_server_cleanup();
