// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_BUFFER

// C includes
#include <string.h>

// JerryScript includes
#include "jerryscript.h"

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_common.h"
#include "zjs_util.h"

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

static const jerry_object_native_info_t buffer_type_info = {
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

static ZJS_DECL_FUNC_ARGS(zjs_buffer_read_bytes, int bytes, bool big_endian)
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

    u32_t offset = 0;
    if (argc >= 1)
        offset = (u32_t)jerry_get_number_value(argv[0]);

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf)
        return zjs_error("buffer not found on read");

    if (offset + bytes > buf->bufsize)
        return zjs_error("read attempted beyond buffer");

    int dir = big_endian ? 1 : -1;
    if (!big_endian)
        offset += bytes - 1;  // start on the big end

    u32_t value = 0;
    for (int i = 0; i < bytes; i++) {
        value <<= 8;
        value |= buf->buffer[offset];
        offset += dir;
    }

    return jerry_create_number(value);
}

static ZJS_DECL_FUNC_ARGS(zjs_buffer_write_bytes, int bytes, bool big_endian)
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
    //             buffer and returns the offset just beyond what was written;
    //             otherwise returns an error

    // args: value[, offset]
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OPTIONAL Z_NUMBER);

    // technically negatives aren't supported but this makes them behave better
    double dval = jerry_get_number_value(argv[0]);
    u32_t value = (u32_t)(dval < 0 ? (s32_t)dval : dval);

    u32_t offset = 0;
    if (argc > 1) {
        offset = (u32_t)jerry_get_number_value(argv[1]);
    }

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf) {
        return zjs_error("buffer not found on write");
    }

    u32_t beyond = offset + bytes;
    if (beyond > buf->bufsize) {
        DBG_PRINT("bufsize %d, write attempted from %d to %d\n",
                  buf->bufsize, offset, beyond);
        return zjs_error("write attempted beyond buffer");
    }

    int dir = big_endian ? -1 : 1;
    if (big_endian)
        offset += bytes - 1;  // start on the little end

    for (int i = 0; i < bytes; i++) {
        buf->buffer[offset] = value & 0xff;
        value >>= 8;
        offset += dir;
    }

    return jerry_create_number(beyond);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint8)
{
    return zjs_buffer_read_bytes(function_obj, this, argv, argc, 1, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint16_be)
{
    return zjs_buffer_read_bytes(function_obj, this, argv, argc, 2, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint16_le)
{
    return zjs_buffer_read_bytes(function_obj, this, argv, argc, 2, false);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint32_be)
{
    return zjs_buffer_read_bytes(function_obj, this, argv, argc, 4, true);
}

static ZJS_DECL_FUNC(zjs_buffer_read_uint32_le)
{
    return zjs_buffer_read_bytes(function_obj, this, argv, argc, 4, false);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint8)
{
    return zjs_buffer_write_bytes(function_obj, this, argv, argc, 1, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint16_be)
{
    return zjs_buffer_write_bytes(function_obj, this, argv, argc, 2, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint16_le)
{
    return zjs_buffer_write_bytes(function_obj, this, argv, argc, 2, false);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint32_be)
{
    return zjs_buffer_write_bytes(function_obj, this, argv, argc, 4, true);
}

static ZJS_DECL_FUNC(zjs_buffer_write_uint32_le)
{
    return zjs_buffer_write_bytes(function_obj, this, argv, argc, 4, false);
}

char zjs_int_to_hex(int value)
{
    // requires: value is between 0 and 15
    //  effects: returns value as a lowercase hex digit 0-9a-f
    if (value < 10)
        return '0' + value;
    return 'a' + value - 10;
}

static ZJS_DECL_FUNC(zjs_buffer_to_string)
{
    // requires: this must be a JS buffer object, if an argument is present it
    //             must be the string 'utf8' (default), 'ascii' or 'hex', as
    //             those are the only supported encodings for now
    //  effects: if the buffer object is found, converts its contents to the
    //             given encoding

    // args: [encoding]
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_OPTIONAL Z_STRING);

    // TODO: add start and end arguments found in Node

    zjs_buffer_t *buf = zjs_buffer_find(this);
    ZJS_ASSERT(buf, "buffer not found");
    if (!buf) {
        return zjs_error("not a buffer");
    }

    const int MAX_ENCODING_LEN = 16;
    jerry_size_t size = MAX_ENCODING_LEN;
    char enc[size];
    const char *encoding = "utf8";
    if (optcount) {
        zjs_copy_jstring(argv[0], enc, &size);
        if (!size) {
            return zjs_error("encoding argument too long");
        }
        encoding = enc;
    }

    if (strequal(encoding, "utf8")) {
        return jerry_create_string_sz_from_utf8((jerry_char_t *)buf->buffer,
                                                buf->bufsize);
    } else if (strequal(encoding, "ascii")) {
        char *str = zjs_malloc(buf->bufsize);
        if (!str) {
            return zjs_error("out of memory");
        }

        int len;
        for (len = 0; len < buf->bufsize; ++len) {
            // strip off high bit if present
            str[len] = buf->buffer[len] & 0x7f;
            if (!str[len]) {
                break;
            }
        }

        jerry_value_t jstr;
        jstr = jerry_create_string_sz_from_utf8((jerry_char_t *)str, len);
        zjs_free(str);
        return jstr;
    } else if (strequal(encoding, "hex")) {
        if (buf && buf->bufsize > 0) {
            char hexbuf[buf->bufsize * 2 + 1];
            for (int i = 0; i < buf->bufsize; i++) {
                int high = (0xf0 & buf->buffer[i]) >> 4;
                int low = 0xf & buf->buffer[i];
                hexbuf[2 * i] = zjs_int_to_hex(high);
                hexbuf[2 * i + 1] = zjs_int_to_hex(low);
            }
            hexbuf[buf->bufsize * 2] = '\0';
            return jerry_create_string((jerry_char_t *)hexbuf);
        }
    } else {
        return zjs_error("unsupported encoding type");
    }
    return zjs_error("buffer is empty");
}

static ZJS_DECL_FUNC(zjs_buffer_copy)
{
    // requires: this must be a JS buffer object and argv[0] a target buffer
    //             object
    //  effects: copies buffer contents w/ optional offsets considered to the
    //             target buffer (following Node v8.9.4 API)

    // args: target, [targetStart], [sourceStart], [sourceEnd]
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_BUFFER,
                               Z_OPTIONAL Z_NUMBER Z_UNDEFINED,
                               Z_OPTIONAL Z_NUMBER Z_UNDEFINED,
                               Z_OPTIONAL Z_NUMBER Z_UNDEFINED);

    zjs_buffer_t *source = zjs_buffer_find(this);
    zjs_buffer_t *target = zjs_buffer_find(argv[0]);
    if (!source || !target) {
        return zjs_error("buffer not found");
    }

    int targetStart = 0;
    int sourceStart = 0;
    int sourceEnd = -1;
    if (optcount >= 1 && !jerry_value_is_undefined(argv[1])) {
        targetStart = (int)jerry_get_number_value(argv[1]);
    }
    if (optcount >= 2 && !jerry_value_is_undefined(argv[2])) {
        sourceStart = (int)jerry_get_number_value(argv[2]);
    }
    if (optcount >= 3 && !jerry_value_is_undefined(argv[3])) {
        sourceEnd = (int)jerry_get_number_value(argv[3]);
    }

    if (sourceEnd == -1) {
        sourceEnd = source->bufsize;
    }

    if (targetStart < 0 || targetStart >= target->bufsize ||
        sourceStart < 0 || sourceStart >= source->bufsize ||
        sourceEnd < 0 || sourceEnd <= sourceStart ||
        sourceEnd > source->bufsize) {
        return zjs_standard_error(RangeError, "invalid copy range", 0, 0);
    }

    if (sourceEnd - sourceStart > target->bufsize - targetStart) {
        return zjs_error("target buffer insufficient");
    }

    int len = sourceEnd - sourceStart;
    memcpy(target->buffer + targetStart, source->buffer + sourceStart, len);
    return jerry_create_number(len);
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

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf) {
        return zjs_error("buffer not found");
    }

    // Check if the encoding string is anything other than utf8
    if (argc > 3) {
        char *encoding = zjs_alloc_from_jstring(argv[3], NULL);
        if (!encoding) {
            return zjs_error("out of memory");
        }

        // ask for one more char than needed to make sure not just prefix match
        const char *utf8_encoding = "utf8";
        int utf8_len = strlen(utf8_encoding);
        int rval = strncmp(encoding, utf8_encoding, utf8_len + 1);
        zjs_free(encoding);
        if (rval != 0) {
            return NOTSUPPORTED_ERROR("only utf8 encoding supported");
        }
    }

    jerry_size_t size = 0;
    char *str = zjs_alloc_from_jstring(argv[0], &size);
    if (!str) {
        return zjs_error("out of memory");
    }

    u32_t offset = 0;
    if (argc > 1)
        offset = (u32_t)jerry_get_number_value(argv[1]);

    u32_t length = buf->bufsize - offset;
    if (argc > 2)
        length = (u32_t)jerry_get_number_value(argv[2]);

    if (length > size) {
        zjs_free(str);
        return zjs_error("requested length larger than string");
    }

    if (offset + length > buf->bufsize) {
        zjs_free(str);
        return zjs_error("string + offset larger than buffer");
    }

    memcpy(buf->buffer + offset, str, length);
    zjs_free(str);

    return jerry_create_number(length);
}

static ZJS_DECL_FUNC(zjs_buffer_fill)
{
    // requires: value - what will be written to buf
    //           offset - where to start writing (Default: 0)
    //           end - offset at which to stop writing (Default: buf.length)
    //           encoding - the character encoding of value. Currently only
    //             supports the default of utf8
    //  effects: writes string to buf at offset according to the character
    //             encoding in encoding.

    // args: data[, offset[, length[, encoding]]]
    ZJS_VALIDATE_ARGS(Z_STRING Z_NUMBER Z_BUFFER, Z_OPTIONAL Z_NUMBER,
                      Z_OPTIONAL Z_NUMBER, Z_OPTIONAL Z_STRING);

    // TODO: support encodings other than 'utf8'

    u32_t num;
    char *source = NULL;
    char *str = NULL;
    u32_t srclen = 0;

    if (jerry_value_is_number(argv[0])) {
        u32_t srcnum = (u32_t)jerry_get_number_value(argv[0]);

        // convert in case of endian difference
        source = (char *)&num;
        source[0] = (0xff000000 & srcnum) >> 24;
        source[1] = (0x00ff0000 & srcnum) >> 16;
        source[2] = (0x0000ff00 & srcnum) >> 8;
        source[3] = 0x000000ff & srcnum;
        srclen = sizeof(u32_t);
    } else if (zjs_value_is_buffer(argv[0])) {
        zjs_buffer_t *srcbuf = zjs_buffer_find(argv[0]);
        source = (char *)srcbuf->buffer;
        srclen = srcbuf->bufsize;
    }

    zjs_buffer_t *buf = zjs_buffer_find(this);
    if (!buf) {
        return zjs_error("buffer not found");
    }

    // Check if the encoding string is anything other than utf8
    if (argc > 3) {
        char *encoding = zjs_alloc_from_jstring(argv[3], NULL);
        if (!encoding) {
            return zjs_error("out of memory");
        }

        // ask for one more char than needed to make sure not just prefix match
        const char *utf8_encoding = "utf8";
        int utf8_len = strlen(utf8_encoding);
        int rval = strncmp(encoding, utf8_encoding, utf8_len + 1);
        zjs_free(encoding);
        if (rval != 0) {
            return NOTSUPPORTED_ERROR("only utf8 encoding supported");
        }
    }

    u32_t offset = 0;
    if (argc > 1)
        offset = (u32_t)jerry_get_number_value(argv[1]);

    u32_t end = buf->bufsize;
    if (argc > 2) {
        end = (u32_t)jerry_get_number_value(argv[2]);
        if (end > buf->bufsize) {
            end = buf->bufsize;
        }
    }

    if (offset >= end) {
        // nothing to do
        return jerry_acquire_value(this);
    }

    if (jerry_value_is_string(argv[0])) {
        jerry_size_t size = 0;
        char *str = zjs_alloc_from_jstring(argv[0], &size);
        if (!str) {
            return zjs_error("out of memory");
        }
        source = str;
        srclen = size;
    }

    u32_t bytes_left = end - offset;
    while (bytes_left > 0) {
        u32_t bytes = srclen;
        if (bytes > bytes_left) {
            bytes = bytes_left;
        }
        memcpy(buf->buffer + offset, source, bytes);
        offset += bytes;
        bytes_left -= bytes;
    }

    zjs_free(str);
    return jerry_acquire_value(this);
}

jerry_value_t zjs_buffer_create(u32_t size, zjs_buffer_t **ret_buf)
{
    // requires: size is size of desired buffer, in bytes
    //  effects: allocates a JS Buffer object, an underlying C buffer, and a
    //             list item to track it; if any of these fail, return an error;
    //             otherwise return the JS object

    // follow Node's Buffer.kMaxLength limits though we don't expose that
    u32_t maxLength = (1UL << 31) - 1;
    if (sizeof(size_t) == 4) {
        // detected 32-bit architecture
        maxLength = (1 << 30) - 1;
    }
    if (size > maxLength) {
        DBG_PRINT("size: %d\n", size);
        return zjs_standard_error(RangeError, "size greater than max length", 0,
                                  0);
    }

    void *buf = zjs_malloc(size);
    zjs_buffer_t *buf_item = (zjs_buffer_t *)zjs_malloc(sizeof(zjs_buffer_t));

    if (!buf || !buf_item) {
        zjs_free(buf);
        zjs_free(buf_item);
        if (ret_buf) {
            *ret_buf = NULL;
        }
        return zjs_error_context("out of memory", 0, 0);
    }

    jerry_value_t buf_obj = zjs_create_object();
    buf_item->buffer = buf;
    buf_item->bufsize = size;

    jerry_set_prototype(buf_obj, zjs_buffer_prototype);
    zjs_obj_add_readonly_number(buf_obj, "length", size);

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
        u32_t unum;
        if (dnum < 0) {
            unum = 0;
        } else if (dnum > 0xffffffff) {
            unum = 0xffffffff;
        } else {
            // round to the nearest integer
            unum = (u32_t)(dnum + 0.5);
        }

        // treat a number argument as a length
        return zjs_buffer_create(unum, NULL);
    } else if (jerry_value_is_array(argv[0])) {
        // treat array argument as byte initializers
        jerry_value_t array = argv[0];
        u32_t len = jerry_get_array_length(array);

        zjs_buffer_t *buf;
        jerry_value_t new_buf = zjs_buffer_create(len, &buf);
        if (buf) {
            for (int i = 0; i < len; i++) {
                ZVAL item = jerry_get_property_by_index(array, i);
                if (jerry_value_is_number(item)) {
                    buf->buffer[i] = (u8_t)jerry_get_number_value(item);
                } else {
                    ERR_PRINT("non-numeric value in array, treating as 0\n");
                    buf->buffer[i] = 0;
                }
            }
        }
        return new_buf;
    } else {
        // FIXME: this should take an encoding too
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
    zjs_obj_add_function(global_obj, "Buffer", zjs_buffer);

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
        { zjs_buffer_copy, "copy" },
        { zjs_buffer_fill, "fill" },
        { zjs_buffer_to_string, "toString" },
        { zjs_buffer_write_string, "write" },
        { NULL, NULL }
    };
    zjs_buffer_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_buffer_prototype, array);
}

void zjs_buffer_cleanup()
{
    jerry_release_value(zjs_buffer_prototype);
}
#endif  // BUILD_MODULE_BUFFER
