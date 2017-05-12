// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_BUFFER

#include <string.h>

// JerryScript includes
#include "jerryscript.h"

// ZJS includes
#include "zjs_util.h"
#include "zjs_buffer.h"

static jerry_value_t zjs_buffer_prototype;

static void zjs_buffer_callback_free(void *handle)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the buffer item
    zjs_buffer_t *item = (zjs_buffer_t *)handle;
    zjs_free(item->buffer);
    zjs_free(item);
}

static const jerry_object_native_info_t buffer_type_info =
{
   .free_cb = zjs_buffer_callback_free
};

bool zjs_value_is_buffer(const jerry_value_t value)
{
    if (jerry_value_is_object(value) && zjs_buffer_find(value)) {
        return true;
    }
    return false;
}

// TODO: Call sites could be replaced with get_object_native_handle directly
zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj)
{
    // requires: obj should be the JS object associated with a buffer, created
    //             in zjs_buffer
    //  effects: looks up obj in our list of known buffer objects and returns
    //             the associated list item struct, or NULL if not found
    zjs_buffer_t *handle;
    const jerry_object_native_info_t *tmp;
    if (jerry_get_object_native_pointer(obj, (void **)&handle, &tmp)) {
        if (tmp == &buffer_type_info) {
            return handle;
        }
    }
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

    // args: offset
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_NUMBER);

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

    // args: value[, offset]
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OPTIONAL Z_NUMBER);

    // technically negatives aren't supported but this makes them behave better
    double dval = jerry_get_number_value(argv[0]);
    uint32_t value = (uint32_t)(dval < 0 ? (int32_t)dval : dval);

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

static ZJS_DECL_FUNC(zjs_buffer_read_uint8)
{
    return zjs_buffer_read_bytes(this, argv, argc, 1, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint16_be)
{
    return zjs_buffer_read_bytes(this, argv, argc, 2, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint16_le)
{
    return zjs_buffer_read_bytes(this, argv, argc, 2, false);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint32_be)
{
    return zjs_buffer_read_bytes(this, argv, argc, 4, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint32_le)
{
    return zjs_buffer_read_bytes(this, argv, argc, 4, false);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint8)
{
    return zjs_buffer_write_bytes(this, argv, argc, 1, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint16_be)
{
    return zjs_buffer_write_bytes(this, argv, argc, 2, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint16_le)
{
    return zjs_buffer_write_bytes(this, argv, argc, 2, false);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint32_be)
{
    return zjs_buffer_write_bytes(this, argv, argc, 4, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint32_le)
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

static ZJS_DECL_FUNC(zjs_buffer_to_string)
{
    // requires: this must be a JS buffer object, if an argument is present it
    //             must be the string 'ascii' or 'hex', as those are the only
    //             supported encodings for now
    //  effects: if the buffer object is found, converts its contents to a hex
    //             string and returns it

    // args: [encoding]
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_STRING);

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (buf && argc == 0) {
        return jerry_create_string((jerry_char_t *)"[Buffer Object]");
    }

    const int MAX_ENCODING_LEN = 16;
    jerry_size_t size = MAX_ENCODING_LEN;
    char encoding[size];
    zjs_copy_jstring(argv[0], encoding, &size);
    if (!size) {
        return zjs_error("zjs_buffer_to_string: encoding argument too long");
    }

    if (strcmp(encoding, "ascii") == 0) {
        buf->buffer[buf->bufsize] = '\0';
        return jerry_create_string((jerry_char_t *)buf->buffer);
    } else if (strcmp(encoding, "hex") == 0) {
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
    } else {
        return zjs_error("zjs_buffer_to_string: unsupported encoding type");
    }
    return zjs_error("zjs_buffer_to_string: buffer is empty");
}

static ZJS_DECL_FUNC(zjs_buffer_write_string)
{
    // requires: string - what will be written to buf
    //           offset - where to start writing (Default: 0)
    //           length - how many bytes to write (Default: buf.length -offset)
    //           encoding - the character encoding of string. Currently only
    //             supports the default of utf8
    //  effects: writes string to buf at offset according to the character
    //             encoding in encoding.

    // args: data[, offset[, length[, encoding]]]
    ZJS_VALIDATE_ARGS(Z_STRING, Z_OPTIONAL Z_NUMBER, Z_OPTIONAL Z_NUMBER,
                      Z_OPTIONAL Z_STRING);

    // Check if the encoding string is anything other than utf8
    if (argc > 3) {
        char *encoding = zjs_alloc_from_jstring(argv[3], NULL);
        if (!encoding) {
            return zjs_error("zjs_buffer_write_string: allocation failure");
        }

        // ask for one more char than needed to make sure not just prefix match
        const char *utf8_encoding = "utf8";
        int utf8_len = strlen(utf8_encoding);
        int rval = strncmp(encoding, utf8_encoding, utf8_len + 1);
        zjs_free(encoding);
        if (rval != 0) {
            return NOTSUPPORTED_ERROR("zjs_buffer_write_string: only utf8 encoding supported");
        }
    }

    jerry_size_t size = 0;
    char *str = zjs_alloc_from_jstring(argv[0], &size);
    if (!str) {
        return zjs_error("zjs_buffer_write_string: string too long");
    }

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf) {
        zjs_free(str);
        return zjs_error("zjs_buffer_write_string: buffer not found");
    }

    uint32_t offset = 0;
    if (argc > 1)
        offset = (uint32_t)jerry_get_number_value(argv[1]);

    uint32_t length = buf->bufsize - offset;
    if (argc > 2)
        length = (uint32_t)jerry_get_number_value(argv[2]);

    if (length > size) {
        zjs_free(str);
        return zjs_error("zjs_buffer_write_string: requested length larger than string");
    }

    if (offset + length > buf->bufsize) {
        zjs_free(str);
        return zjs_error("zjs_buffer_write_string: string + offset larger than buffer");
    }

    memcpy(buf->buffer + offset, str, length);
    zjs_free(str);

    return jerry_create_number(length);
}

jerry_value_t zjs_buffer_create(uint32_t size, zjs_buffer_t **ret_buf)
{
    // requires: size is size of desired buffer, in bytes
    //  effects: allocates a JS Buffer object, an underlying C buffer, and a
    //             list item to track it; if any of these fail, return an error;
    //             otherwise return the JS object

    // follow Node's Buffer.kMaxLength limits though we don't expose that
    uint32_t maxLength = (1UL << 31) - 1;
    if (sizeof(size_t) == 4) {
        // detected 32-bit architecture
        maxLength = (1 << 30) - 1;
    }
    if (size > maxLength) {
#ifdef DEBUG_BUILD
        return RANGE_ERROR("size greater than max length");
#else
        return RANGE_ERROR("");
#endif
    }

    void *buf = zjs_malloc(size);
    zjs_buffer_t *buf_item = (zjs_buffer_t *)zjs_malloc(sizeof(zjs_buffer_t));

    if (!buf || !buf_item) {
        zjs_free(buf);
        zjs_free(buf_item);
        if (ret_buf) {
            *ret_buf = NULL;
        }
        return zjs_error("out of memory");
    }

    jerry_value_t buf_obj = jerry_create_object();
    buf_item->buffer = buf;
    buf_item->bufsize = size;

    jerry_set_prototype(buf_obj, zjs_buffer_prototype);
    zjs_obj_add_readonly_number(buf_obj, size, "length");

    // watch for the object getting garbage collected, and clean up
    jerry_set_object_native_pointer(buf_obj, buf_item, &buffer_type_info);
    if (ret_buf) {
        *ret_buf = buf_item;
    }
    return buf_obj;
}

// Buffer constructor
static ZJS_DECL_FUNC(zjs_buffer)
{
    // requires: single argument can be a numeric size in bytes, an array of
    //             uint8s, or a string
    //  effects: constructs a new JS Buffer object, and an associated buffer
    //             tied to it through a zjs_buffer_t struct stored in a global
    //             list

    // args: initial size or initialization data
    ZJS_VALIDATE_ARGS(Z_NUMBER Z_ARRAY Z_STRING);

    if (jerry_value_is_number(argv[0])) {
        double dnum = jerry_get_number_value(argv[0]);
        uint32_t unum;
        if (dnum < 0) {
            unum = 0;
        }
        else if (dnum > 0xffffffff) {
            unum = 0xffffffff;
        }
        else {
            // round to the nearest integer
            unum = (uint32_t)(dnum + 0.5);
        }

        // treat a number argument as a length
        return zjs_buffer_create(unum, NULL);
    }
    else if (jerry_value_is_array(argv[0])) {
        // treat array argument as byte initializers
        jerry_value_t array = argv[0];
        uint32_t len = jerry_get_array_length(array);

        zjs_buffer_t *buf;
        jerry_value_t new_buf = zjs_buffer_create(len, &buf);
        if (buf) {
            for (int i = 0; i < len; i++) {
                ZVAL item = jerry_get_property_by_index(array, i);
                if (jerry_value_is_number(item)) {
                    buf->buffer[i] = (uint8_t)jerry_get_number_value(item);
                } else {
                    ERR_PRINT("non-numeric value in array, treating as 0");
                    buf->buffer[i] = 0;
                }
            }
        }
        return new_buf;
    }
    else {
        // treat string argument as initializer
        jerry_size_t size = 0;
        char *str = zjs_alloc_from_jstring(argv[0], &size);
        if (!str) {
            return zjs_error("could not allocate string");
        }

        zjs_buffer_t *buf;
        jerry_value_t new_buf = zjs_buffer_create(size, &buf);
        if (buf) {
            memcpy(buf->buffer, str, size);
        }

        zjs_free(str);
        return new_buf;
    }
}

void zjs_buffer_init()
{
    ZVAL global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_buffer, "Buffer");

    zjs_native_func_t array[] = {
        { zjs_buffer_read_uint8, "readUInt8" },
        { zjs_buffer_write_uint8, "writeUInt8" },
        { zjs_buffer_read_uint16_be, "readUInt16BE" },
        { zjs_buffer_write_uint16_be, "writeUInt16BE" },
        { zjs_buffer_read_uint16_le, "readUInt16LE" },
        { zjs_buffer_write_uint16_le, "writeUInt16LE" },
        { zjs_buffer_read_uint32_be, "readUInt32BE" },
        { zjs_buffer_write_uint32_be, "writeUInt32BE" },
        { zjs_buffer_read_uint32_le, "readUInt32LE" },
        { zjs_buffer_write_uint32_le, "writeUInt32LE" },
        { zjs_buffer_to_string, "toString" },
        { zjs_buffer_write_string, "write" },
        { NULL, NULL }
    };
    zjs_buffer_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_buffer_prototype, array);
}

void zjs_buffer_cleanup()
{
    jerry_release_value(zjs_buffer_prototype);
}
#endif // BUILD_MODULE_BUFFER
