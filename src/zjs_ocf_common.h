// Copyright (c) 2016-2017, Intel Corporation.

#include "jerryscript.h"

#include "zjs_common.h"
#include "zjs_ocf_client.h"
#include "zjs_util.h"

#include "oc_api.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
//#include "port/oc_signal_main_loop.h"
#include "port/oc_clock.h"

struct props_handle {
    jerry_value_t props_array;
    uint32_t size;
    char **names_array;
};

#define TYPE_IS_NUMBER 0
#define TYPE_IS_INT    1
#define TYPE_IS_UINT   2

#define OCF_MAX_DEVICE_ID_LEN 36
#define OCF_MAX_RES_TYPE_LEN  32
#define OCF_MAX_RES_PATH_LEN  64
#define OCF_MAX_URI_LEN       64
#define OCF_MAX_PROP_NAME_LEN 16

// Helper to get the string from a jerry_value_t
#define ZJS_GET_STRING(jval, name, maxlen)       \
    jerry_size_t name##_size = maxlen; \
    char name[name##_size]; \
    zjs_copy_jstring(jval, name, &name##_size);

#define REJECT(promise, err_name, err_msg, handler) \
    handler = new_ocf_handler(NULL); \
    zjs_make_promise(promise, post_ocf_promise, handler); \
    jerry_value_t error = make_ocf_error(err_name, err_msg, NULL); \
    zjs_reject_promise(promise, &error, 1); \
    jerry_release_value(error);

/*
 * Test if value at index is a double or integer
 *
 * @param val           JerryScript value to test
 *
 * @return              0=number, 1=int, 2=uint
 */
int zjs_ocf_is_int(jerry_value_t val);

/*
 * Encode all properties in props_object into a CborEncoder for an
 * iotivity-constrained callback.
 *
 * @param props_object  JerryScript object containing properties to encode
 * @param encoder       CborEncoder to encode the properties into
 * @param root          Flag if this is the root object or a sub-object
 *
 * @return              Handle to free the properties later
 */
void *zjs_ocf_props_setup(jerry_value_t props_object,
                          CborEncoder *encoder,
                          bool root);

/*
 * Free properties after a call to zjs_ocf_props_setup()
 *
 * @param h             Handler returned from zjs_ocf_props_setup()
 */
void zjs_ocf_free_props(void *h);

/**
 * Routine to call into iotivity-constrained
 *
 * @return              Time (ms) until next event
 */
int32_t main_poll_routine(void* handle);

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
