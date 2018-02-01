// Copyright (c) 2016-2018, Intel Corporation.
// Zephyr.js required its own versions of these macros from
// iotivity-constrained.
// in order to use the cbor_* API's in a dynamic way.

#ifndef __zjs_ocf_encoder_h__
#define __zjs_ocf_encoder_h__

// C includes
#include <stdbool.h>
#include <stdint.h>

// Zephyr includes
#include "deps/tinycbor/src/cbor.h"
#include <config.h>

// OCF includes
#include "oc_helpers.h"

extern CborEncoder g_encoder, root_map, links_array;
extern CborError g_err;

/*
 * Start encoding an object
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child object (CborEncoder *)
 */
#define zjs_rep_start_object(parent, key) \
    do {                                  \
    g_err |= cbor_encoder_create_map(parent, key, CborIndefiniteLength)

/*
 * Finish encoding an object
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child object (CborEncoder *)
 */
#define zjs_rep_end_object(parent, key)                 \
    g_err |= cbor_encoder_close_container(parent, key); \
    }                                                   \
    while (0)

/*
 * Set an objects name
 *
 * @param object        Parent object (CborEncoder *)
 * @param key           Child object (CborEncoder *)
 * @param name          Name of object (char *)
 */
#define zjs_rep_set_object(object, key, name)                         \
    if (name)                                                         \
        g_err |= cbor_encode_text_string(object, name, strlen(name)); \
    zjs_rep_start_object(object, key)

/*
 * Close/End an object
 *
 * @param object        Parent object (CborEncoder *)
 * @param key           Child object (CborEncoder *)
 */
#define zjs_rep_close_object(object, key) zjs_rep_end_object(object, key)

/*
 * Start encoding to the root object. This should only be called once at the
 * start of encoding.
 */
#define zjs_rep_start_root_object() \
    g_err |=                        \
        cbor_encoder_create_map(&g_encoder, &root_map, CborIndefiniteLength)

/*
 * Finish encoding the root object.
 */
#define zjs_rep_end_root_object() \
    g_err |= cbor_encoder_close_container(&g_encoder, &root_map)

/*
 * Start encoding an array object
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child array object (CborEncoder *)
 */
#define zjs_rep_start_array(parent, key) \
    do {                                 \
    g_err |= cbor_encoder_create_array(parent, key, CborIndefiniteLength)

/*
 * Finish encoding an array obect
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child array object (CborEncoder *)
 */
#define zjs_rep_end_array(parent, key)                  \
    g_err |= cbor_encoder_close_container(parent, key); \
    }                                                   \
    while (0)

/*
 * Set an array's name
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child array object (CborEncoder *)
 * @param name          Name of array object (char *)
 */
#define zjs_rep_set_array(object, key, name)                      \
    g_err |= cbor_encode_text_string(object, name, strlen(name)); \
    zjs_rep_start_array(object, key)

/*
 * Close/end an array object
 *
 * @param parent        Parent object (CborEncoder *)
 * @param key           Child array object (CborEncoder *)
 */
#define zjs_rep_close_array(object, key) zjs_rep_end_array(object, key)

/*
 * Encode a boolean value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_boolean(object, key, value)                         \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_boolean(object, value);                    \
    } while (0)

/*
 * Encode a double value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_double(object, key, value)                          \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_double(object, value);                     \
    } while (0)

/*
 * Encode a integer value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_int(object, key, value)                             \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_int(object, value);                        \
    } while (0)

/*
 * Encode a unsigned integer value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_uint(object, key, value)                            \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_uint(object, value);                       \
    } while (0)

/*
 * Encode a text string value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_text_string(object, key, value)                     \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_text_string(object, value, strlen(value)); \
    } while (0)

/*
 * Encode a byte string value
 *
 * @param object        Parent object
 * @param key           Name of boolean property
 * @param value         Value
 */
#define zjs_rep_set_byte_string(object, key, value)                     \
    do {                                                                \
        if (key)                                                        \
            g_err |= cbor_encode_text_string(object, key, strlen(key)); \
        g_err |= cbor_encode_byte_string(object, value, strlen(value)); \
    } while (0)

#endif
