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

static zjs_buffer_t *zjs_buffers = NULL;

// TODO: this could probably be replaced more efficiently now that there is a
//   get_native_handle API
zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj)
{
    // requires: obj should be the JS object associated with a buffer, created
    //             in zjs_buffer
    //  effects: looks up obj in our list of known buffer objects and returns
    //             the associated list item struct
    for (zjs_buffer_t *buf = zjs_buffers; buf; buf = buf->next)
        if (buf->obj == obj)
            return buf;
    return NULL;
}

static jerry_value_t zjs_buffer_read_bytes(const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc,
                                           int bytes, bool big_endian)
{
    // requires: this is a JS buffer object created with zjs_buffer_create,
    //             argv[0] should be an offset into the buffer, but will treat
    //             offset as 0 if not given, as node.js seems to
    //           bytes is the number of bytes to read (expects 1, 2, or 4)
    //           big_endian true reads the bytes in big endian order, false in
    //             little endian order
    //  effects: reads bytes from the buffer associated with this JS object, if
    //             found, at the given offset, if within the bounds of the
    //             buffer; otherwise returns an error
    if (argc >= 1 && !jerry_value_is_number(argv[0]))
        return zjs_error("zjs_buffer_read_bytes: invalid argument");

    uint32_t offset = 0;
    if (argc >= 1)
        offset = (uint32_t)jerry_get_number_value(argv[0]);

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf)
        return zjs_error("zjs_buffer_read_bytes: buffer not found on read");

    if (offset + bytes > buf->bufsize)
        return zjs_error("zjs_buffer_read_bytes: read attempted beyond buffer");

    int dir = big_endian ? 1 : -1;
    if (!big_endian)
        offset += bytes - 1;  // start on the big end

    uint32_t value = 0;
    for (int i = 0; i < bytes; i++) {
        value <<= 8;
        value |= buf->buffer[offset];
        offset += dir;
    }

    return jerry_create_number(value);
}

static jerry_value_t zjs_buffer_write_bytes(const jerry_value_t this,
                                            const jerry_value_t argv[],
                                            const jerry_length_t argc,
                                            int bytes, bool big_endian)
{
    // requires: this is a JS buffer object created with zjs_buffer_create,
    //             argv[0] must be the value to be written, argv[1] should be
    //             an offset into the buffer, but will treat offset as 0 if not
    //             given, as node.js seems to
    //           bytes is the number of bytes to write (expects 1, 2, or 4)
    //           big_endian true writes the bytes in big endian order, false in
    //             little endian order
    //  effects: writes bytes into the buffer associated with this JS object, if
    //             found, at the given offset, if within the bounds of the
    //             buffer; otherwise returns an error
    if (argc < 1 || !jerry_value_is_number(argv[0]) ||
        (argc >= 2 && !jerry_value_is_number(argv[1]))) {
        return zjs_error("zjs_buffer_write_bytes: invalid argument");
    }

    uint32_t value = jerry_get_number_value(argv[0]);

    uint32_t offset = 0;
    if (argc > 1)
        offset = (uint32_t)jerry_get_number_value(argv[1]);

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf)
        return zjs_error("zjs_buffer_write_bytes: buffer not found on write");

    if (offset + bytes > buf->bufsize)
        return zjs_error("zjs_buffer_write_bytes: write attempted beyond buffer");

    int dir = big_endian ? -1 : 1;
    if (big_endian)
        offset += bytes - 1;  // start on the little end

    for (int i = 0; i < bytes; i++) {
        buf->buffer[offset] = value & 0xff;
        value >>= 8;
        offset += dir;
    }

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_buffer_read_uint8(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    return zjs_buffer_read_bytes(this, argv, argc, 1, true);
}

static jerry_value_t zjs_buffer_read_uint16_be(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_read_bytes(this, argv, argc, 2, true);
}

static jerry_value_t zjs_buffer_read_uint16_le(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_read_bytes(this, argv, argc, 2, false);
}

static jerry_value_t zjs_buffer_read_uint32_be(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_read_bytes(this, argv, argc, 4, true);
}

static jerry_value_t zjs_buffer_read_uint32_le(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_read_bytes(this, argv, argc, 4, false);
}

static jerry_value_t zjs_buffer_write_uint8(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    return zjs_buffer_write_bytes(this, argv, argc, 1, true);
}

static jerry_value_t zjs_buffer_write_uint16_be(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_write_bytes(this, argv, argc, 2, true);
}

static jerry_value_t zjs_buffer_write_uint16_le(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_write_bytes(this, argv, argc, 2, false);
}

static jerry_value_t zjs_buffer_write_uint32_be(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_write_bytes(this, argv, argc, 4, true);
}

static jerry_value_t zjs_buffer_write_uint32_le(const jerry_value_t function_obj,
                                               const jerry_value_t this,
                                               const jerry_value_t argv[],
                                               const jerry_length_t argc)
{
    return zjs_buffer_write_bytes(this, argv, argc, 4, false);
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

    zjs_buffer_t *buf = zjs_buffer_find(this);
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
    zjs_buffer_t **pItem = &zjs_buffers;
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
    zjs_buffer_t *buf = zjs_buffer_find(this);

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
    zjs_buffer_t *buf_item =
        (zjs_buffer_t *)zjs_malloc(sizeof(zjs_buffer_t));

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
    zjs_obj_add_function(buf_obj, zjs_buffer_read_uint16_be, "readUInt16BE");
    zjs_obj_add_function(buf_obj, zjs_buffer_write_uint16_be, "writeUInt16BE");
    zjs_obj_add_function(buf_obj, zjs_buffer_read_uint16_le, "readUInt16LE");
    zjs_obj_add_function(buf_obj, zjs_buffer_write_uint16_le, "writeUInt16LE");
    zjs_obj_add_function(buf_obj, zjs_buffer_read_uint32_be, "readUInt32BE");
    zjs_obj_add_function(buf_obj, zjs_buffer_write_uint32_be, "writeUInt32BE");
    zjs_obj_add_function(buf_obj, zjs_buffer_read_uint32_le, "readUInt32LE");
    zjs_obj_add_function(buf_obj, zjs_buffer_write_uint32_le, "writeUInt32LE");
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
        zjs_buffer_t *buf = zjs_buffer_find(new_buf_obj);
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
        zjs_buffer_t *buf = zjs_buffer_find(new_buf_obj);

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
    jerry_release_value(global_obj);
}
#endif // BUILD_MODULE_BUFFER
