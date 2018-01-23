// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_board_h__
#define __zjs_board_h__

#include "jerryscript.h"

// ZJS includes
#include "zjs_common.h"

// return codes from zjs_find_* functions
#define FIND_PIN_INVALID    -1
#define FIND_PIN_FAILURE    -2
#define FIND_DEVICE_FAILURE -3

// max length of a named pin like IO3, PWM0, LED0
#define NAMED_PIN_MAX_LEN 8

#ifdef BUILD_MODULE_GPIO
/**
 * Finds the Zephyr pin corresponding to a JavaScript GPIO pin name or number
 *
 * @param jspin        JrS number or string pin name
 * @param device_name  Pointer to a buffer to receive driver name
 * @param len          Length of device_name buffer
 *
 * @return ZJS pin number, or negative FIND_* error number above if not found
 */
int zjs_board_find_gpio(jerry_value_t jspin, char *device_name, int len);
#endif

#ifdef BUILD_MODULE_AIO
/**
 * Finds the Zephyr pin corresponding to a JavaScript AIO pin name or number
 *
 * @param jspin        JrS number or string pin name
 * @param device_name  Pointer to a buffer to receive driver name
 * @param len          Length of device_name buffer
 *
 * @return ZJS pin number, or negative FIND_* error number above if not found
 */
int zjs_board_find_aio(jerry_value_t jspin, char *device_name, int len);
#endif

#ifdef BUILD_MODULE_PWM
/**
 * Finds the Zephyr pin corresponding to a JavaScript PWM pin name or number
 *
 * @param jspin        JrS number or string pin name
 * @param device_name  Pointer to a buffer to receive driver name
 * @param len          Length of device_name buffer
 *
 * @return ZJS pin number, or negative FIND_* error number above if not found
 */
int zjs_board_find_pwm(jerry_value_t jspin, char *device_name, int len);
#endif

// wrappers for unit tests
#ifdef ZJS_LINUX_BUILD
int wrap_split_pin_name(const char *name, char *prefix, int *number);
#endif

#endif  // __zjs_board_h__
