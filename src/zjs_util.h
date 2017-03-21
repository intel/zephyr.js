// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_util_h__
#define __zjs_util_h__

// The util code is only for the X86 side

#include <stdlib.h>

#include "jerry-api.h"
#include "zjs_common.h"
#include "zjs_error.h"

#define ZJS_UNDEFINED jerry_create_undefined()

/**
 * Call malloc but if it fails, run JerryScript garbage collection and retry
 *
 * @param size  Number of bytes to allocate
 */
void *zjs_malloc_with_retry(size_t size);

#ifdef ZJS_LINUX_BUILD
#define zjs_malloc(sz) malloc(sz)
#define zjs_free(ptr) free(ptr)
#else
#ifdef ZJS_TRACE_MALLOC
#define zjs_malloc(sz) ({void *zjs_ptr = zjs_malloc_with_retry(sz); ZJS_PRINT("%s:%d: allocating %lu bytes (%p)\n", __func__, __LINE__, (uint32_t)sz, zjs_ptr); zjs_ptr;})
#define zjs_free(ptr) (ZJS_PRINT("%s:%d: freeing %p\n", __func__, __LINE__, ptr), free(ptr))
#else
#define zjs_malloc(sz) ({void *zjs_ptr = zjs_malloc_with_retry(sz); if (!zjs_ptr) {ERR_PRINT("malloc failed");} zjs_ptr;})
#define zjs_free(ptr) free(ptr)
#endif  // ZJS_TRACE_MALLOC
#endif  // ZJS_LINUX_BUILD

void zjs_set_property(const jerry_value_t obj, const char *name,
                      const jerry_value_t prop);
void zjs_set_readonly_property(const jerry_value_t obj, const char *name,
                               const jerry_value_t prop);
jerry_value_t zjs_get_property (const jerry_value_t obj, const char *name);
bool zjs_delete_property(const jerry_value_t obj, const char *str);

typedef struct zjs_native_func {
    void *function;
    const char *name;
} zjs_native_func_t;

/**
 * Add a series of functions described in array funcs to obj.
 *
 * @param obj    JerryScript object to add the functions to
 * @param funcs  Array of zjs_native_func_t, terminated with {NULL, NULL}
 */
void zjs_obj_add_functions(jerry_value_t obj, zjs_native_func_t *funcs);

void zjs_obj_add_boolean(jerry_value_t obj, bool flag, const char *name);
void zjs_obj_add_readonly_boolean(jerry_value_t obj, bool flag,
                                  const char *name);
void zjs_obj_add_function(jerry_value_t obj, void *function, const char *name);
void zjs_obj_add_object(jerry_value_t parent, jerry_value_t child,
                        const char *name);
void zjs_obj_add_string(jerry_value_t obj, const char *str, const char *name);
void zjs_obj_add_readonly_string(jerry_value_t obj, const char *str,
                                 const char *name);
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

bool zjs_hex_to_byte(const char *buf, uint8_t *byte);

void zjs_default_convert_pin(uint32_t orig, int *dev, int *pin);

uint16_t zjs_compress_32_to_16(uint32_t num);
uint32_t zjs_uncompress_16_to_32(uint16_t num);

void zjs_print_error_message(jerry_value_t error);

/**
 * Release a jerry_value_t passed by reference
 */
void zjs_free_value(const jerry_value_t *value);

/**
 * Macro to declare a jerry_value_t and have it released automatically
 *
 * If you're going to be returning the value from a function, you usually don't
 * want this, as ownership will transfer to the caller (unless you acquire it on
 * return). However, if you have error return paths that should release the
 * value, then it might still be a good idea.
 *
 * Also, don't use this for a variable that holds a C copy of a value that was
 * passed into you, as those are owned by the caller.
 */
#define ZVAL const jerry_value_t __attribute__ ((__cleanup__(zjs_free_value)))

/**
 * A non-const version of ZVAL
 *
 * This is for when you need to initialize a ZVAL from more than one path. It
 * should be used sparingly, because this is less safe; it's possible to
 * overwrite a value and forget to release the old one.
 */
#define ZVAL_MUTABLE jerry_value_t __attribute__ ((__cleanup__(zjs_free_value)))

//
// ztypes (for argument validation)
//

// Z_OPTIONAL means the argument isn't required, this must come first if present
#define Z_OPTIONAL  "?"

// Z_ANY matches any type (i.e. ignores it) - only makes sense for required arg
#define Z_ANY       "a"
// the rest all match specific type by calling jerry_value_is_* function
#define Z_ARRAY     "b"
#define Z_BOOL      "c"
#define Z_FUNCTION  "d"
#define Z_NULL      "e"
#define Z_NUMBER    "f"
// NOTE: Z_OBJECT will match arrays and functions too, because they are objects
#define Z_OBJECT    "g"
#define Z_STRING    "h"
#define Z_UNDEFINED "i"

enum {
    ZJS_VALID_REQUIRED,
    ZJS_VALID_OPTIONAL,
    ZJS_SKIP_OPTIONAL,
    ZJS_INTERNAL_ERROR = -1,
    ZJS_INVALID_ARG = -2,
    ZJS_INSUFFICIENT_ARGS = -3
};

int zjs_validate_args(const char *expectations[], const jerry_length_t argc,
                      const jerry_value_t argv[]);
/**
 * Macro to validate existing argv based on a list of expected argument types.
 *
 * NOTE: Expects argc and argv to exist as in a JerryScript native function.
 *
 * @param optcount  A pointer to an int to receive count of optional args found,
 *                    or NULL if not needed.
 * @param offset    Integer offset of arg in argv to start with (normally 0)
 * @param typestr   Each remaining comma-separated argument to the macro
 *                    corresponds to an argument in argv; each argument should
 *                    be a space-separated list of "ztypes" from defines above.
 *
 * Example: ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OBJECT Z_NULL, Z_OPTIONAL Z_FUNCTION);
 * This requires argv[0] to be a number type, argv[1] to be an object type
 *   or null,  and argv[2] may or may not exist, but if it does it must be a
 *   function type. (Implicitly, argc will have to be at least 2.) Arguments
 *   beyond what are specified are allowed and ignored.
 */
#define ZJS_VALIDATE_ARGS_FULL(optcount, offset, ...)                   \
    const char *zjs_expectations[] = { __VA_ARGS__, NULL };             \
    int optcount = zjs_validate_args(zjs_expectations, argc - offset,   \
                                     argv + offset);                    \
    if (optcount <= ZJS_INVALID_ARG) {                                  \
        return TYPE_ERROR("invalid arguments");                         \
    }

// Use this if you need an offset
#define ZJS_VALIDATE_ARGS_OFFSET(offset, ...)           \
    ZJS_VALIDATE_ARGS_FULL(zjs_validate_rval, offset, __VA_ARGS__)

// Use this if you need the number of optional args found
#define ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, ...)       \
    ZJS_VALIDATE_ARGS_FULL(optcount, 0, __VA_ARGS__)

// Normally use this as a shortcut
#define ZJS_VALIDATE_ARGS(...)                      \
    ZJS_VALIDATE_ARGS_FULL(zjs_validate_rval, 0, __VA_ARGS__)

#endif  // __zjs_util_h__
