// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_buffer_h__
#define __zjs_buffer_h__

#include "jerryscript.h"

/** Initialize the buffer module, or reinitialize after cleanup */
void zjs_buffer_init();

/** Release resources held by the buffer module */
void zjs_buffer_cleanup();

// FIXME: We should make this private and have accessor methods
typedef struct zjs_buffer {
    jerry_value_t obj;  // weak reference
    uint8_t *buffer;
    uint32_t bufsize;
    struct zjs_buffer *next;
} zjs_buffer_t;

zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj);


/**
 * Create a new Buffer object
 *
 * @param size     Buffer size in bytes
 * @param ret_buf  Output pointer to receive new buffer handle, or NULL
 *
 * @return  New JS Buffer or Error object, and sets *ret_buf to C handle or
 *            NULL, if given
 */
jerry_value_t zjs_buffer_create(uint32_t size, zjs_buffer_t **ret_buf);

#endif  // __zjs_buffer_h__
