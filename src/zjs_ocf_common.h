// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <stdio.h>

// JerryScript includes
#include "jerryscript.h"

// ZJS includes
#include "zjs_common.h"
#include "zjs_ocf_client.h"
#include "zjs_util.h"

// OCF includes
#include "oc_api.h"
//#include "port/oc_signal_main_loop.h"
#include "port/oc_clock.h"

#define OCF_MAX_DEVICE_ID_LEN 42
#define OCF_MAX_RES_TYPE_LEN  32
#define OCF_MAX_RES_PATH_LEN  64
#define OCF_MAX_URI_LEN       64
#define OCF_MAX_PROP_NAME_LEN 16

// Helper to get the string from a jerry_value_t
#define ZJS_GET_STRING(jval, name, maxlen) \
    jerry_size_t name##_size = maxlen;     \
    char name[name##_size];                \
    zjs_copy_jstring(jval, name, &name##_size);

#define REJECT(err_name, err_msg)                           \
    jerry_value_t promise = jerry_create_promise();         \
    ZVAL error = make_ocf_error(err_name, err_msg, NULL);   \
    jerry_resolve_or_reject_promise(promise, error, false); \
    return promise;

/*
 * Encode all properties in 'object' into the global CborEncoder
 *
 * @param object        JerryScript object containing properties to encode
 */
void zjs_ocf_encode_value(jerry_value_t object);

/*
 * Decode properties from an OCF response into a jerry_value_t
 *
 * @param data          Data from OCF response
 *
 * @return              Jerry object matching 'data'
 */
jerry_value_t zjs_ocf_decode_value(oc_rep_t *data);

/**
 * Routine to call into iotivity-constrained
 *
 * @return              Time (ms) until next event
 */
s32_t main_poll_routine(void *handle);

/**
 * Set the 'uuid' property in the device object. This API is required because
 * we dont get the UUID until after the device object is created/initialized.
 *
 * @param uuid          UUID obtained from iotivity-constrained system
 */
void zjs_set_uuid(char *uuid);

/*
 * Object returned from require('ocf')
 */
jerry_value_t zjs_ocf_init();

/*
 * Cleanup for OCF object
 */
void zjs_ocf_cleanup();
