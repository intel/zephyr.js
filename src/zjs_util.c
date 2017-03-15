// Copyright (c) 2016-2017, Intel Corporation.

#include <string.h>

// ZJS includes
#include "zjs_util.h"

void zjs_set_property(const jerry_value_t obj, const char *name,
                      const jerry_value_t prop)
{
    // requires: obj is an object, name is a property name string
    //  effects: create a new field in the object named name, and
    //             set it to prop
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_set_property(obj, jname, prop);
    jerry_release_value(jname);
}

void zjs_set_readonly_property(const jerry_value_t obj, const char *name,
                               const jerry_value_t prop)
{
    // requires: obj is an object, name is a property name string
    //  effects: create a new readonly field in the object named name, and
    //             set it to prop
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_property_descriptor_t pd;
    jerry_init_property_descriptor_fields(&pd);
    pd.is_writable_defined = true;
    pd.is_writable = false;
    pd.is_configurable_defined = true;
    pd.is_configurable = true;
    pd.is_value_defined = true;
    pd.value = prop;
    jerry_define_own_property(obj, jname, &pd);
    jerry_free_property_descriptor_fields(&pd);
    jerry_release_value(jname);
}

jerry_value_t zjs_get_property(const jerry_value_t obj, const char *name)
{
    // requires: obj is an object, name is a property name string
    //  effects: looks up the property name in obj, and returns it; the value
    //             will be owned by the caller and must be released
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t rval = jerry_get_property(obj, jname);
    jerry_release_value(jname);
    return rval;
}

bool zjs_delete_property(const jerry_value_t obj, const char *name)
{
    // requires: obj is an object, name is a property name string
    //  effects: looks up the property name in obj, and deletes it; return true
    //             if success, false otherwise
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    bool rval = jerry_delete_property(obj, jname);
    jerry_release_value(jname);
    return rval;
}

void zjs_obj_add_functions(jerry_value_t obj, zjs_native_func_t *funcs)
{
    // requires: obj is an existing JS object
    //           funcs is an array of zjs_native_func_t structs, terminated by a
    //             struct with a NULL function field
    //  effects: adds all of the described functions to obj with given names
    for (zjs_native_func_t *map = funcs; map->function; map++) {
        zjs_obj_add_function(obj, map->function, map->name);
    }
}

void zjs_obj_add_boolean(jerry_value_t obj, bool flag, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jbool = jerry_create_boolean(flag);
    jerry_set_property(obj, jname, jbool);
    jerry_release_value(jname);
    jerry_release_value(jbool);
}

void zjs_obj_add_readonly_boolean(jerry_value_t obj, bool flag,
                                  const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to value
    zjs_set_readonly_property(obj, name, jerry_create_boolean(flag));
}

void zjs_obj_add_function(jerry_value_t obj, void *func, const char *name)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function

    // NOTE: The docs on this function make it look like func obj should be
    //   released before we return, but in a loop of 25k buffer creates there
    //   seemed to be no memory leak. Reconsider with future intelligence.
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jfunc = jerry_create_external_function(func);
    if (jerry_value_is_function(jfunc)) {
        jerry_set_property(obj, jname, jfunc);
    }
    jerry_release_value(jname);
    jerry_release_value(jfunc);
}

void zjs_obj_add_object(jerry_value_t parent, jerry_value_t child,
                        const char *name)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_set_property(parent, jname, child);
    jerry_release_value(jname);
}

void zjs_obj_add_string(jerry_value_t obj, const char *str, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to str
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jstr = jerry_create_string((const jerry_char_t *)str);
    jerry_set_property(obj, jname, jstr);
    jerry_release_value(jname);
    jerry_release_value(jstr);
}

void zjs_obj_add_readonly_string(jerry_value_t obj, const char *str,
                                 const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to str
    zjs_set_readonly_property(obj, name, jerry_create_string((const jerry_char_t *)str));
}

void zjs_obj_add_number(jerry_value_t obj, double num, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to nval
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jnum = jerry_create_number(num);
    jerry_set_property(obj, jname, jnum);
    jerry_release_value(jname);
    jerry_release_value(jnum);
}

void zjs_obj_add_readonly_number(jerry_value_t obj, double num,
                                 const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new readonly field in parent named name, set to num
    zjs_set_readonly_property(obj, name, jerry_create_number(num));
}

bool zjs_obj_get_boolean(jerry_value_t obj, const char *name, bool *flag)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a boolean
    jerry_value_t value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_boolean(value)) {
        *flag = jerry_get_boolean_value(value);
        rval = true;
    }

    jerry_release_value(value);
    return rval;
}

bool zjs_obj_get_string(jerry_value_t obj, const char *name, char *buffer,
                        int len)
{
    // requires: obj is an existing JS object, buffer can receive the string,
    //             len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, and can fit into the given buffer, copies it plus
    //             a null terminator into buffer and returns true; otherwise,
    //             returns false
    jerry_value_t value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_string(value)) {
        jerry_size_t size = len;
        zjs_copy_jstring(value, buffer, &size);
        if (size)
            rval = true;
    }

    jerry_release_value(value);
    return rval;
}

bool zjs_obj_get_double(jerry_value_t obj, const char *name, double *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = jerry_get_number_value(value);
        rval = true;
    }

    jerry_release_value(value);
    return rval;
}

bool zjs_obj_get_uint32(jerry_value_t obj, const char *name, uint32_t *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = (uint32_t)jerry_get_number_value(value);
        rval = true;
    }

    jerry_release_value(value);
    return rval;
}

bool zjs_obj_get_int32(jerry_value_t obj, const char *name, int32_t *num)
{
    // requires: obj is an existing JS object
    //  effects: retrieves field specified by name as a int32
    jerry_value_t value = zjs_get_property(obj, name);
    bool rval = false;

    if (!jerry_value_has_error_flag(value) && jerry_value_is_number(value)) {
        *num = (int32_t)jerry_get_number_value(value);
        rval = true;
    }

    jerry_release_value(value);
    return rval;
}

void zjs_copy_jstring(jerry_value_t jstr, char *buffer, jerry_size_t *maxlen)
{
    jerry_size_t size = jerry_get_string_size(jstr);
    jerry_size_t len = 0;
    if (*maxlen > size)
        len = jerry_string_to_char_buffer(jstr, (jerry_char_t *)buffer, size);
    buffer[len] = '\0';
    *maxlen = len;
}

char *zjs_alloc_from_jstring(jerry_value_t jstr, jerry_size_t *maxlen)
{
    jerry_size_t size = jerry_get_string_size(jstr);
    char *buffer = zjs_malloc(size + 1);
    if (!buffer) {
        ERR_PRINT("allocation failed (%lu bytes)\n", size + 1);
        return NULL;
    }

    jerry_size_t len;
    len = jerry_string_to_char_buffer(jstr, (jerry_char_t *)buffer, size);
    buffer[len] = '\0';

    if (maxlen) {
        if (*maxlen && *maxlen < len) {
            DBG_PRINT("string limited to %lu bytes\n", *maxlen);
            buffer[*maxlen] = '\0';
        }
        else {
            *maxlen = len;
        }
    }

    return buffer;
}

bool zjs_hex_to_byte(const char *buf, uint8_t *byte)
{
    // requires: buf is a string with at least two hex chars
    //  effects: converts the first two hex chars in buf to a number and
    //             returns it in *byte; returns true on success, false otherwise
    uint8_t num = 0;
    for (int i=0; i<2; i++) {
        num <<= 4;
        if (buf[i] >= 'A' && buf[i] <= 'F')
            num += buf[i] - 'A' + 10;
        else if (buf[i] >= 'a' && buf[i] <= 'f')
            num += buf[i] - 'a' + 10;
        else if (buf[i] >= '0' && buf[i] <= '9')
            num += buf[i] - '0';
        else return false;
    }
    *byte = num;
    return true;
}

void zjs_default_convert_pin(uint32_t orig, int *dev, int *pin) {
    // effects: reads top three bits of the bottom byte of orig and writes them
    //            to dev and the bottom five bits and writes them to pin; thus
    //            up to eight devices are supported and up to 32 pins each; but
    //            if orig is 0xff, returns 0 for dev and -1 for pin
    if (orig == 0xff) {
        *dev = 0;
        *pin = -1;
    }
    else {
        *dev = (orig & 0xe0) >> 5;
        *pin = orig & 0x1f;
    }
}

// when accuracy isn't as important as space
uint16_t zjs_compress_32_to_16(uint32_t num)
{
    int zeroes = __builtin_clzl(num);
    if (sizeof(unsigned long) > 4) {
        zeroes -= (sizeof(unsigned long) - 4) * 8;
    }

    if (zeroes >= 17)
        return (uint16_t)num;

    // take the top 16 bits
    uint16_t compressed = (num << zeroes) >> 16;

    // clear the bottom five bits to save leading zeroes
    compressed &= 0xffe0;

    // save the number of zeroes in bottom five bits
    return compressed | (uint16_t)zeroes;
}

uint32_t zjs_uncompress_16_to_32(uint16_t num)
{
    if ((num & 0x8000) == 0)
        return (uint32_t)num;

    // take top 11 bits
    uint32_t uncompressed = (num & 0xffe0) >> 5;

    // recover the number of leading zeroes
    int zeroes = num & 0x1f;

    // shift back up to the right point
    return uncompressed << (21 - zeroes);
}

#define MAX_ERROR_NAME_LENGTH       32
#define MAX_ERROR_MESSAGE_LENGTH    128

void zjs_print_error_message(jerry_value_t error)
{
    const char *uncaught = "Uncaught exception: ";

    uint32_t size;
    char *message = NULL;
    jerry_value_t err_name = zjs_get_property(error, "name");
    if (!jerry_value_is_string(err_name)) {
        ERR_PRINT("%s(no name)\n", uncaught);
        jerry_release_value(err_name);
        // we should never get here.
        return;
    }
    jerry_value_t err_msg = zjs_get_property(error, "message");
    if (!jerry_value_is_string(err_msg)) {
        ERR_PRINT("%s(no message)\n", uncaught);
        jerry_release_value(err_name);
        jerry_release_value(err_msg);
        // we should never get here.
        return;
    }

    size = MAX_ERROR_NAME_LENGTH;
    char name[size];

    zjs_copy_jstring(err_name, name, &size);
    if (!size) {
        ERR_PRINT("%s(name too long)\n", uncaught);
        jerry_release_value(err_name);
        jerry_release_value(err_msg);
        return;
    }
    jerry_release_value(err_name);

    message = zjs_alloc_from_jstring(err_msg, NULL);

    jerry_release_value(err_msg);

    if (message) {
        ERR_PRINT("%s%s: %s\n", uncaught, name, message);
        zjs_free(message);
    } else {
        ERR_PRINT("%s%s\n", uncaught, name);
    }
}

bool zjs_true(const jerry_value_t value)
{
    // return true for any value
    return true;
}

typedef bool (*zjs_type_test)(const jerry_value_t);

// NOTE: These must match the order of Z_ANY to Z_UNDEFINED in zjs_util.h
zjs_type_test zjs_type_map[] = {
    zjs_true,
    jerry_value_is_array,
    jerry_value_is_boolean,
    jerry_value_is_function,
    jerry_value_is_null,
    jerry_value_is_number,
    jerry_value_is_object,
    jerry_value_is_string,
    jerry_value_is_undefined
};

static int zjs_validate_arg(const char *expectation, jerry_value_t arg)
{
    // requires: If this argument is optional, first char of expectation must
    //             be '?'; then additional chars are one of "abflnosu", meaning
    //             array, boolean, float, nul(l), number, object, string or
    //             undefined, and it must be null-terminated.
    //  effects: returns a value from enum above
    bool optional = false;
    int index = 0;
    if (expectation[0] == Z_OPTIONAL[0]) {
        optional = true;
        ++index;
    }

    while (expectation[index] != '\0') {
        // NOTE: This relies on Z_ANY to Z_UNDEFINED being consecutive and
        //   Z_UNDEFINED being the last one
        int type_index = expectation[index] - Z_ANY[0];
        if (type_index < 0 || expectation[index] > Z_UNDEFINED[0]) {
            ERR_PRINT("invalid argument type: '%c'\n", expectation[index]);
            return ZJS_INTERNAL_ERROR;
        }

        if (zjs_type_map[type_index](arg)) {
            return optional ? ZJS_VALID_OPTIONAL : ZJS_VALID_REQUIRED;
        }

        ++index;
    }

    return optional ? ZJS_SKIP_OPTIONAL : ZJS_INVALID_ARG;
}

int zjs_validate_args(const char *expectations[], const jerry_length_t argc,
                      const jerry_value_t argv[])
{
    // effects: returns number of optional arguments found, or a negative
    //            number on error
    int expect_index = 0, arg_index = 0, opt_args = 0;
    while (expectations[expect_index] != NULL && arg_index < argc) {
        int rval = zjs_validate_arg(expectations[expect_index],
                                    argv[arg_index]);
        switch (rval) {
        case ZJS_VALID_OPTIONAL:
            ++opt_args;
        case ZJS_VALID_REQUIRED:
            ++arg_index;
        case ZJS_SKIP_OPTIONAL:
            ++expect_index;
            break;

        default:
            return rval;
        }
    }

    // check for any more required args
    while (expectations[expect_index] != NULL) {
        if (expectations[expect_index][0] != '?')
            return ZJS_INSUFFICIENT_ARGS;
        ++expect_index;
    }

    if (arg_index < argc) {
        DBG_PRINT("API received %lu unexpected arg(s)\n", argc - arg_index);
    }
    return opt_args;
}
