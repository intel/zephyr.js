// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_buffer_h__
#define __zjs_buffer_h__

#include "jerry-api.h"

void zjs_buffer_init();

typedef struct zjs_buffer {
    jerry_value_t obj;
    uint8_t *buffer;
    uint32_t bufsize;
    struct zjs_buffer *next;
} zjs_buffer_t;

zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj);

jerry_value_t zjs_buffer_create(uint32_t size);

void zjs_buffer_free_buffers();

#endif  // __zjs_buffer_h__
