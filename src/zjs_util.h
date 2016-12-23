// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_util_h__
#define __zjs_util_h__

// The util code is only for the X86 side

#include "jerry-api.h"
#include "zjs_common.h"

#define ZJS_UNDEFINED jerry_create_undefined()

#ifdef ZJS_LINUX_BUILD
#include <stdlib.h>
#define zjs_malloc(sz) malloc(sz)
#define zjs_free(ptr) free((void *)ptr)
#else
#ifdef ZJS_TRACE_MALLOC
#include <zephyr.h>
#define zjs_malloc(sz) ({void *zjs_ptr = k_malloc(sz); ZJS_PRINT("%s:%d: allocating %lu bytes (%p)\n", __func__, __LINE__, (uint32_t)sz, zjs_ptr); zjs_ptr;})
#define zjs_free(ptr) (ZJS_PRINT("%s:%d: freeing %p\n", __func__, __LINE__, ptr), k_free(ptr))
#else
#include <zephyr.h>
#define zjs_malloc(sz) k_malloc(sz)
#define zjs_free(ptr) k_free(ptr)
#endif  // ZJS_TRACE_MALLOC
#endif  // ZJS_LINUX_BUILD

void zjs_set_property(const jerry_value_t obj, const char *str,
                      const jerry_value_t prop);
jerry_value_t zjs_get_property (const jerry_value_t obj, const char *str);

typedef struct zjs_native_func {
    void *function;
    const char *name;
} zjs_native_func_t;

/**
 * Add a series of functions described in array funcs to obj
 * @param obj    JerryScript object to add the functions to
 * @param funcs  Array of zjs_native_func_t, terminated with {NULL, NULL}
 */
void zjs_obj_add_functions(jerry_value_t obj, zjs_native_func_t *funcs);

void zjs_obj_add_boolean(jerry_value_t obj, bool flag, const char *name);
void zjs_obj_add_function(jerry_value_t obj, void *function, const char *name);
void zjs_obj_add_object(jerry_value_t parent, jerry_value_t child,
                        const char *name);
void zjs_obj_add_string(jerry_value_t obj, const char *str, const char *name);
void zjs_obj_add_number(jerry_value_t obj, double num, const char *name);
void zjs_obj_add_readonly_number(jerry_value_t obj, double num,
                                 const char *name);

bool zjs_obj_get_boolean(jerry_value_t obj, const char *name, bool *flag);
bool zjs_obj_get_string(jerry_value_t obj, const char *name, char *buffer,
                        int len);
bool zjs_obj_get_double(jerry_value_t obj, const char *name, double *num);
bool zjs_obj_get_uint32(jerry_value_t obj, const char *name, uint32_t *num);
bool zjs_obj_get_int32(jerry_value_t obj, const char *name, int32_t *num);

/**
 * Copy a JerryScript string into a supplied char * buffer.
 *
 * @param jstr    A JerryScript string value.
 * @param buffer  A char * buffer with at least *maxlen bytes.
 * @param maxlen  Pointer to a maximum size to be written to the buffer. If the
 *                  string size with a null terminator would exceed *maxlen,
 *                  only a null terminator will be written to the buffer and
 *                  *maxlen will be set to 0. If the string is successfully
 *                  copied, *maxlen will be set to the bytes copied (not
 *                  counting the null terminator). If *maxlen is 0, behavior is
 *                  undefined.
 */
void zjs_copy_jstring(jerry_value_t jstr, char *buffer, jerry_size_t *maxlen);

/**
 * Allocate a char * buffer on the heap and copy the JerryScript string to it.
 *
 * @param jstr    A JerryScript string value.
 * @param maxlen  Pointer to a maximum size for the returned string. If NULL or
 *                  pointing to 0, there is no limit to the string size
 *                  returned. If not NULL, the actual length of the string will
 *                  be written to *maxlen. If the call succeeds, the buffer
 *                  returned will be truncated to the given maxlen with a null
 *                  terminator. You can use zjs_copy_jstring if you'd rather
 *                  fail than truncate on error.
 * @return A new null-terminated string (which must be freed with zjs_free) or
 *          NULL on failure.
 */
char *zjs_alloc_from_jstring(jerry_value_t jstr, jerry_size_t *maxlen);

bool zjs_hex_to_byte(char *buf, uint8_t *byte);

void zjs_default_convert_pin(uint32_t orig, int *dev, int *pin);

uint16_t zjs_compress_32_to_16(uint32_t num);
uint32_t zjs_uncompress_16_to_32(uint16_t num);

jerry_value_t zjs_error(const char *error);

#endif  // __zjs_util_h__
