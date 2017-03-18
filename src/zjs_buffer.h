// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_buffer_h__
#define __zjs_buffer_h__

#include "jerry-api.h"

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

jerry_value_t zjs_buffer_create(uint32_t size);

#endif  // __zjs_buffer_h__
