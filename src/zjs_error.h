// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_error_h__
#define __zjs_error_h__

// JerryScript includes
#include "jerryscript.h"

typedef enum zjs_error_type {
    NetworkError,
    NotSupportedError,
    RangeError,
    SecurityError,
    SyntaxError,
    SystemError,
    TimeoutError,
    TypeError,
    Error
} zjs_error_type_t;

/** Initialize the error module, or reinitialize after cleanup. */
void zjs_error_init();

/** Release resources held by the error module. */
void zjs_error_cleanup();

jerry_value_t zjs_error_with_func(jerry_value_t this,
                                  jerry_value_t func,
                                  zjs_error_type_t type,
                                  const char *message);

jerry_value_t zjs_custom_error_with_func(jerry_value_t this,
                                         jerry_value_t func,
                                         const char *name,
                                         const char *message);

/**
 * Create an error object to return from an API.
 *
 * @param name A custom name for the error.
 * @param message String for additional detail about the error.
 *
 * @return A new Error object with the given name and message set.
 */
jerry_value_t zjs_custom_error(const char *name, const char *message,
                               jerry_value_t this, jerry_value_t func);

/**
 * Create an error object to return from an API.
 *
 * @param type An error type from zjs_error_type enum.
 * @param message String for additional detail about the error.
 *
 * @return A new Error or subclass object.
 */
jerry_value_t zjs_standard_error(zjs_error_type_t type, const char *message,
                                 jerry_value_t this, jerry_value_t func);

#define ZJS_STD_ERROR(type, msg) \
    (zjs_standard_error((type), (msg), this, function_obj))

// Error Context
//
// The error "context" referred to in this function is meant to be the JS
//   function we're running on behalf of, and the object that function was
//   called on. So the zjs_error() function has been updated to automatically
//   use this and function_obj parameters that are present in any ZJS_DECL_FUNC.
//
// Use this zjs_error_context function when you need to manually pass in the
//   context. The two situations where this might happen are when you have no
//   context for some reason (pass 0 for both), or when you're not directly in
//   a ZJS_DECL_FUNC and they may have different names.
//
// The error reporting uses this context to print descriptive information for
//   the user after errors such as:
// "In function MyGPIOObjects.led0.write():"
//   to indicate where the error occurred since we can't currently provide line
//   numbers.
#define zjs_error_context(msg, this, func) \
    (zjs_standard_error(Error, (msg), (this), (func)))

// The below functions when inside a ZJS_DECL_FUNC JS binding; they require
//   that 'this' and 'function_obj' be defined.
#define zjs_error(msg)          (ZJS_STD_ERROR(Error, (msg)))

#define SECURITY_ERROR(msg)     (ZJS_STD_ERROR(SecurityError, (msg)))
#define NOTSUPPORTED_ERROR(msg) (ZJS_STD_ERROR(NotSupportedError, (msg)))
#define SYNTAX_ERROR(msg)       (ZJS_STD_ERROR(SyntaxError, (msg)))
#define TYPE_ERROR(msg)         (ZJS_STD_ERROR(TypeError, (msg)))
#define RANGE_ERROR(msg)        (ZJS_STD_ERROR(RangeError, (msg)))
#define TIMEOUT_ERROR(msg)      (ZJS_STD_ERROR(TimeoutError, (msg)))
#define NETWORK_ERROR(msg)      (ZJS_STD_ERROR(NetworkError, (msg)))
#define SYSTEM_ERROR(msg)       (ZJS_STD_ERROR(SystemError, (msg)))

/*
 * These macros expects function_obj and this to exist in a JerryScript native
 * function.
 */
#define ZJS_ERROR(msg) (zjs_error_with_func(this, function_obj, Error, (msg)))
#define ZJS_CUSTOM_ERROR(name, msg) \
    (zjs_custom_error_with_func(this, function_obj, (name), (msg)))

#endif  // __zjs_error_h__
