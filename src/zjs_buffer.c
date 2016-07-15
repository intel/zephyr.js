// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <string.h>

// JerryScript includes
#include "jerry-api.h"

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"

static struct zjs_buffer_t *zjs_buffers = NULL;

// TODO: this could probably be replaced more efficiently now that there is a
//   get_native_handle API
struct zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj_val)
{
    // requires: obj should be the JS object associated with a buffer, created
    //             in zjs_buffer
    //  effects: looks up obj in our list of known buffer objects and returns
    //             the associated list item struct
    for (struct zjs_buffer_t *buf = zjs_buffers; buf; buf = buf->next)
        if (buf->obj == obj_val)
            return buf;
    return NULL;
}

static jerry_value_t zjs_buffer_read_uint8(const jerry_value_t function_obj_val,
                                           const jerry_value_t this_val,
                                           const jerry_value_t args_p[],
                                           const jerry_length_t args_cnt)
{
    // requires: this_val is a JS buffer object created with zjs_buffer_create,
    //             expects argument of an offset into the buffer, but will
    //             treat offset as 0 if not given, as node.js seems to
    //  effects: reads single byte value from  the buffer associated with the
    //             this_p JS object, if found, at the given offset, if within
    //             the bounds of the buffer; otherwise returns an error
    if (args_cnt >= 1 && !jerry_value_is_number(args_p[0])) {
        PRINT("zjs_buffer_read_uint8: invalid argument\n");
        return zjs_error("zjs_buffer_read_uint8: invalid argument");
    }

    uint32_t offset = 0;
    if (args_cnt >= 1)
        offset = (uint32_t)jerry_get_number_value(args_p[0]);

    struct zjs_buffer_t *buf = zjs_buffer_find(this_val);
    if (buf) {
        if (offset >= buf->bufsize) {
            PRINT("zjs_buffer_read_uint8: read beyond end of buffer\n");
            return zjs_error("zjs_buffer_read_uint8: read beyond end of buffer");
        }
        uint8_t value = buf->buffer[offset];

        return jerry_create_number(value);
    }

    PRINT("zjs_buffer_read_uint8: read called on a buffer not in list");  // Fatal
    return zjs_error("zjs_buffer_read_uint8: read called on a buffer not in list");
}

static jerry_value_t zjs_buffer_write_uint8(const jerry_value_t function_obj_val,
                                            const jerry_value_t this_val,
                                            const jerry_value_t args_p[],
                                            const jerry_length_t args_cnt)
{
    // requires: this_val is a JS buffer object created with zjs_buffer_create,
    //             expects arguments of the value to write and an offset
    //             into the buffer, but will treat offset as 0 if not given,
    //             as node.js seems to
    //  effects: writes single byte value into the buffer associated with the
    //             this_p JS object, if found, at the given offset, if within
    //             the bounds of the buffer; otherwise returns an error
    if (args_cnt < 1 || !jerry_value_is_number(args_p[0]) ||
        (args_cnt >= 2 && !jerry_value_is_number(args_p[1]))) {
        PRINT("zjs_buffer_write_uint8: invalid argument\n");
        return zjs_error("zjs_buffer_write_uint8: invalid argument");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(args_p[0]);

    uint32_t offset = 0;
    if (args_cnt > 1)
        offset = (uint32_t)jerry_get_number_value(args_p[1]);

    struct zjs_buffer_t *buf = zjs_buffer_find(this_val);
    if (buf) {
        if (offset >= buf->bufsize) {
            PRINT("zjs_buffer_write_uint8: write beyond end of buffer\n");
            return zjs_error("zjs_buffer_write_uint8: write beyond end of buffer");
        }
        buf->buffer[offset] = value;

        return jerry_create_number(offset + 1);
    }

    PRINT("zjs_buffer_write_uint8: write called on a buffer not in list");  // Fatal
    return zjs_error("zjs_buffer_write_uint8: read called on a buffer not in list");
}

char zjs_int_to_hex(int value) {
    // requires: value is between 0 and 15
    //  effects: returns value as a lowercase hex digit 0-9a-f
    if (value < 10)
        return '0' + value;
    return 'a' + value - 10;
}

static jerry_value_t zjs_buffer_to_string(const jerry_value_t function_obj_val,
                                          const jerry_value_t this_val,
                                          const jerry_value_t args_p[],
                                          const jerry_length_t args_cnt)
{
    // requires: this_val must be a JS buffer object, if an argument is present it
    //             must be the string 'hex', as that is the only supported
    //             encoding for now
    //  effects: if the buffer object is found, converts its contents to a hex
    //             string and returns it in ret_val_p
    if (args_cnt > 1 || (args_cnt == 1 && !jerry_value_is_string(args_p[0]))) {
        PRINT("zjs_buffer_to_string: invalid argument\n");
        return zjs_error("zjs_buffer_to_string: invalid argument");
    }

    struct zjs_buffer_t *buf = zjs_buffer_find(this_val);
    if (buf && args_cnt == 0) {
        return jerry_create_string((jerry_char_t *)"[Buffer Object]");
    }

    char encoding[16];
    jerry_size_t sz = jerry_get_string_size(args_p[0]);
    if (sz > 15)
        sz = 15;
    int len = jerry_string_to_char_buffer(args_p[0],
                                          (jerry_char_t *)encoding, sz);
    encoding[len] = '\0';

    if (strcmp(encoding, "hex")) {
        PRINT("zjs_buffer_to_string: unsupported encoding type\n");
        return zjs_error("zjs_buffer_to_string: unsupported encoding type");
    }

    if (buf && buf->bufsize > 1) {
        char hexbuf[buf->bufsize * 2 + 1];
        for (int i=0; i<buf->bufsize; i++) {
            int high = (0xf0 & buf->buffer[i]) >> 4;
            int low = 0xf & buf->buffer[i];
            hexbuf[2*i] = zjs_int_to_hex(high);
            hexbuf[2*i+1] = zjs_int_to_hex(low);
        }
        hexbuf[buf->bufsize * 2] = '\0';
        return jerry_create_string((jerry_char_t *)hexbuf);
    }

    return zjs_error("zjs_buffer_to_string: buffer is empty");
}

static void zjs_buffer_callback_free(uintptr_t handle)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the callback list item for the given pin object
    struct zjs_buffer_t **pItem = &zjs_buffers;
    while (*pItem) {
        if ((uintptr_t)*pItem == handle) {
            task_free((*pItem)->buffer);
            *pItem = (*pItem)->next;
            task_free((void *)handle);
        }
        pItem = &(*pItem)->next;
    }
}

jerry_value_t zjs_buffer_create(uint32_t size)
{
    // requires: size is size of desired buffer, in bytes
    //  effects: allocates a JS Buffer object, an underlying C buffer, and a
    //             list item to track it; if any of these fail, free them all
    //             and return NULL, otherwise return the JS object
    jerry_value_t buf_obj = jerry_create_object();
    void *buf = task_malloc(size);
    struct zjs_buffer_t *buf_item =
        (struct zjs_buffer_t *)task_malloc(sizeof(struct zjs_buffer_t));

    if (!buf_obj || !buf || !buf_item) {
        PRINT("zjs_buffer_create: unable to allocate buffer\n");
        jerry_release_value(buf_obj);
        task_free(buf);
        task_free(buf_item);
        return NULL;
    }

    buf_item->obj = buf_obj;
    buf_item->buffer = buf;
    buf_item->bufsize = size;
    buf_item->next = zjs_buffers;
    zjs_buffers = buf_item;

    zjs_obj_add_number(buf_obj, size, "length");
    zjs_obj_add_function(buf_obj, zjs_buffer_read_uint8, "readUInt8");
    zjs_obj_add_function(buf_obj, zjs_buffer_write_uint8, "writeUInt8");
    zjs_obj_add_function(buf_obj, zjs_buffer_to_string, "toString");

    // TODO: sign up to get callback when the object is freed, then free the
    //   buffer and remove it from the list

    // watch for the object getting garbage collected, and clean up
    jerry_set_object_native_handle(buf_obj, (uintptr_t)buf_item,
                                   zjs_buffer_callback_free);

    return buf_obj;
}

// Buffer constructor
static jerry_value_t zjs_buffer(const jerry_value_t function_obj_val,
                                const jerry_value_t this_val,
                                const jerry_value_t args_p[],
                                const jerry_length_t args_cnt)
{
    // requires: single argument is a numeric size in bytes; soon will also
    //             support an array argument to initialize the buffer
    //  effects: constructs a new JS Buffer object, and an associated buffer
    //             tied to it through a zjs_buffer_t struct stored in a global
    //             list
    if (args_cnt != 1 || !jerry_value_is_number(args_p[0])) {
        PRINT("zjs_buffer: invalid argument\n");
        return zjs_error("zjs_buffer: invalid argument");
    }

    uint32_t size = (uint32_t)jerry_get_number_value(args_p[0]);
    return zjs_buffer_create(size);
}

void zjs_buffer_init()
{
    jerry_value_t global_obj_val = jerry_get_global_object();
    zjs_obj_add_function(global_obj_val, zjs_buffer, "Buffer");
}
