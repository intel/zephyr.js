// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_BUFFER
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>

// JerryScript includes
#include "jerry-api.h"

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"

static struct zjs_buffer_t *zjs_buffers = NULL;

// TODO: this could probably be replaced more efficiently now that there is a
//   get_native_handle API
struct zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj)
{
    // requires: obj should be the JS object associated with a buffer, created
    //             in zjs_buffer
    //  effects: looks up obj in our list of known buffer objects and returns
    //             the associated list item struct
    for (struct zjs_buffer_t *buf = zjs_buffers; buf; buf = buf->next)
        if (buf->obj == obj)
            return buf;
    return NULL;
}

static jerry_value_t zjs_buffer_read_uint8(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    // requires: this is a JS buffer object created with zjs_buffer_create,
    //             expects argument of an offset into the buffer, but will
    //             treat offset as 0 if not given, as node.js seems to
    //  effects: reads single byte value from the buffer associated with the
    //             this JS object, if found, at the given offset, if within
    //             the bounds of the buffer; otherwise returns an error
    if (argc >= 1 && !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_buffer_read_uint8: invalid argument");

    uint32_t offset = 0;
    if (argc >= 1)
        offset = (uint32_t)jerry_get_number_value(argv[0]);

    struct zjs_buffer_t *buf = zjs_buffer_find(this);
    if (buf) {
        if (offset >= buf->bufsize)
            return zjs_error("zjs_buffer_read_uint8: read beyond end of buffer");
        uint8_t value = buf->buffer[offset];

        return jerry_create_number(value);
    }

    return zjs_error("zjs_buffer_read_uint8: read called on a buffer not in list");
}

static jerry_value_t zjs_buffer_write_uint8(const jerry_value_t function_obj,
                                            const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc)
{
    // requires: this is a JS buffer object created with zjs_buffer_create,
    //             expects arguments of the value to write and an offset
    //             into the buffer, but will treat offset as 0 if not given,
    //             as node.js seems to
    //  effects: writes single byte value into the buffer associated with the
    //             this JS object, if found, at the given offset, if within
    //             the bounds of the buffer; otherwise returns an error
    if (argc < 1 || !jerry_value_is_number(argv[0]) ||
        (argc >= 2 && !jerry_value_is_number(argv[1]))) {
        return zjs_error("zjs_buffer_write_uint8: invalid argument");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(argv[0]);

    uint32_t offset = 0;
    if (argc > 1)
        offset = (uint32_t)jerry_get_number_value(argv[1]);

    struct zjs_buffer_t *buf = zjs_buffer_find(this);
    if (buf) {
        if (offset >= buf->bufsize)
            return zjs_error("zjs_buffer_write_uint8: write beyond end of buffer");
        buf->buffer[offset] = value;

        return jerry_create_number(offset + 1);
    }

    return zjs_error("zjs_buffer_write_uint8: read called on a buffer not in list");
}

char zjs_int_to_hex(int value) {
    // requires: value is between 0 and 15
    //  effects: returns value as a lowercase hex digit 0-9a-f
    if (value < 10)
        return '0' + value;
    return 'a' + value - 10;
}

static jerry_value_t zjs_buffer_to_string(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc)
{
    // requires: this must be a JS buffer object, if an argument is present it
    //             must be the string 'hex', as that is the only supported
    //             encoding for now
    //  effects: if the buffer object is found, converts its contents to a hex
    //             string and returns it in ret_val_p
    if (argc > 1 || (argc == 1 && !jerry_value_is_string(argv[0])))
        return zjs_error("zjs_buffer_to_string: invalid argument");

    struct zjs_buffer_t *buf = zjs_buffer_find(this);
    if (buf && argc == 0) {
        return jerry_create_string((jerry_char_t *)"[Buffer Object]");
    }

    const int maxlen = 16;
    char encoding[maxlen];
    jerry_size_t sz = jerry_get_string_size(argv[0]);
    if (sz >= maxlen) {
        return zjs_error("zjs_buffer_to_string: encoding argument too long");
    }
    int len = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)encoding,
                                          sz);
    encoding[len] = '\0';

    if (strcmp(encoding, "hex"))
        return zjs_error("zjs_buffer_to_string: unsupported encoding type");

    if (buf && buf->bufsize > 0) {
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
            zjs_free((*pItem)->buffer);
            *pItem = (*pItem)->next;
            zjs_free((void *)handle);
        }
        pItem = &(*pItem)->next;
    }
}

static jerry_value_t zjs_buffer_write_string(const jerry_value_t function_obj_val,
                                             const jerry_value_t this,
                                             const jerry_value_t argv[],
                                             const jerry_length_t argc)
{
    // requires: string - what will be written to buf
    //           offset - where to start writing (Default: 0)
    //           length - how many bytes to write (Default: buf.length -offset)
    //           encoding - the character encoding of string. Currently only supports
    //           the default of utf8
    // effects: writes string to buf at offset according to the character encoding in encoding.

    if (argc < 1 || !jerry_value_is_string(argv[0]) ||
        (argc > 1 && !jerry_value_is_number(argv[1])) ||
        (argc > 2 && !jerry_value_is_number(argv[2])) ||
        (argc > 3 && !jerry_value_is_string(argv[3])))
        return zjs_error("zjs_buffer_write_string: invalid argument");

    // Check if the encoding string is anything other than utf8
    if (argc > 3) {
        jerry_value_t arg4 = argv[3];
        jerry_size_t arg4_sz = jerry_get_string_size(arg4);

        if (arg4_sz > 4096) {
            return zjs_error("zjs_buffer_write_string: encoding arg string is too long");
        }

        char arg4_str[arg4_sz];
        char utf8_str[4];
        strcpy(utf8_str, "utf8");
        uint8_t utf8 = strcmp(arg4_str, utf8_str);

        if (utf8 != 0) {
            return zjs_error("zjs_buffer_write_string: only utf8 encoding is supported");
        }
    }

    uint32_t offset = 0;
    jerry_value_t arg = argv[0];
    jerry_size_t sz = jerry_get_string_size(arg);
    struct zjs_buffer_t *buf = zjs_buffer_find(this);

    if (sz > 4096) {
        return zjs_error("zjs_buffer_write_string: string is too long for the buffer");
    }

    char str[sz];

    if (!buf) {
        return zjs_error("zjs_buffer_write_string: buffer pointer not found");
    }

    if (argc > 1)
        offset = (uint32_t)jerry_get_number_value(argv[1]);

    uint32_t length = buf->bufsize - offset;

    if (argc > 2)
        length = (uint32_t)jerry_get_number_value(argv[2]);

    if (offset + length > buf->bufsize) {        
        return zjs_error("zjs_buffer_write_string: string + offset is larger than the buffer");
    }

    jerry_string_to_char_buffer(arg, str, sz);

    memcpy(&buf->buffer[offset], &str[0], length);

    return jerry_create_number(length);
}

jerry_value_t zjs_buffer_create(uint32_t size)
{
    // requires: size is size of desired buffer, in bytes
    //  effects: allocates a JS Buffer object, an underlying C buffer, and a
    //             list item to track it; if any of these fail, free them all
    //             and return NULL, otherwise return the JS object
    jerry_value_t buf_obj = jerry_create_object();
    void *buf = zjs_malloc(size);
    struct zjs_buffer_t *buf_item =
        (struct zjs_buffer_t *)zjs_malloc(sizeof(struct zjs_buffer_t));

    if (!buf_obj || !buf || !buf_item) {
        PRINT("zjs_buffer_create: unable to allocate buffer\n");
        jerry_release_value(buf_obj);
        zjs_free(buf);
        zjs_free(buf_item);
        return ZJS_UNDEFINED;
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
    zjs_obj_add_function(buf_obj, zjs_buffer_write_string, "write");

    // TODO: sign up to get callback when the object is freed, then free the
    //   buffer and remove it from the list

    // watch for the object getting garbage collected, and clean up
    jerry_set_object_native_handle(buf_obj, (uintptr_t)buf_item,
                                   zjs_buffer_callback_free);

    return buf_obj;
}

// Buffer constructor
static jerry_value_t zjs_buffer(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc)
{
    // requires: single argument can be a numeric size in bytes, an array of uint8,
    //           or a string.
    //  effects: constructs a new JS Buffer object, and an associated buffer
    //             tied to it through a zjs_buffer_t struct stored in a global
    //             list
    if (argc != 1 ||
        !(jerry_value_is_number(argv[0]) ||
        jerry_value_is_array(argv[0]) ||
        jerry_value_is_string(argv[0])))
        return zjs_error("zjs_buffer: invalid argument");

    if (jerry_value_is_number(argv[0])) {
        // If passed a number, use that to allocate a buffer with a length of that number
        uint32_t size = (uint32_t)jerry_get_number_value(argv[0]);
        return zjs_buffer_create(size);
    } else if (jerry_value_is_array(argv[0])){
        // If passed an array, allocate the memory and fill it with the array value
        jerry_value_t array = argv[0];
        uint32_t arr_size = jerry_get_array_length(array);
        jerry_value_t new_buf_obj = zjs_buffer_create(arr_size);
        struct zjs_buffer_t *buf = zjs_buffer_find(new_buf_obj);
        jerry_value_t array_item;

        if (buf) {
            if (arr_size > buf->bufsize) {
                return zjs_error("zjs_buffer: write beyond end of buffer");
            }

            for (int i = 0; i < arr_size; i++) {
                array_item = jerry_get_property_by_index(array, i);
                if (jerry_value_is_number(array_item)) {
                    buf->buffer[i] = (uint8_t)jerry_get_number_value(array_item);
                } else {
                    return zjs_error("zjs_buffer: buffer only supports numeric values in an array");
                }
            }
        }
        return new_buf_obj;
    } else {
        // If passed a string, convert it into a char array and copy it to the buffer.
        jerry_value_t arg = argv[0];
        jerry_size_t sz = jerry_get_string_size(arg);
        jerry_value_t new_buf_obj = zjs_buffer_create(sz);
        struct zjs_buffer_t *buf = zjs_buffer_find(new_buf_obj);

        if (buf) {
            jerry_string_to_char_buffer(arg, (char*)buf->buffer, sz);
        } else {
            return zjs_error("zjs_buffer: unable to find string buffer");
        }

        return new_buf_obj;
    }
}

void zjs_buffer_init()
{
    jerry_value_t global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_buffer, "Buffer");
}
#endif // BUILD_MODULE_BUFFER
