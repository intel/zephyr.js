// Copyright (c) 2016-2018, Intel Corporation.

#ifndef __zjs_common_h__
#define __zjs_common_h__

// This file includes code common to both X86 and ARC

// C includes
#include <stdio.h>

#ifdef ZJS_LINUX_BUILD
#include "zjs_linux_port.h"
#else
// Zephyr includes
#if CONFIG_ARC
#include <misc/printk.h>
#endif
#include <zephyr/types.h>
#include "zjs_zephyr_port.h"
#endif

#ifdef CONFIG_ARC
#define ZJS_PRINT printk
#else
#define ZJS_PRINT printf
#endif

/**
 * Return a pointer to the filename portion of a string plus one parent dir
 *
 * @param filepath  A valid null-terminated string
 * @returns A pointer to a substring of filepath, namely the character right
 *   after the second to last slash, or filepath itself if not found.
 */
char *zjs_shorten_filepath(char *filepath);

// enable to use asserts even in release mode, e.g. for terse debugging
#if 0
#define USE_ASSERT
#endif

#ifdef DEBUG_BUILD
int zjs_get_sec(void);
int zjs_get_ms(void);

#if defined(ZJS_VERBOSE) && ZJS_VERBOSE >= 3
// Verbosity level 3 shows timestamp and function/line
#define DBG_PRINT                                                             \
    ZJS_PRINT("\n%u.%3.3u %s:%d %s():\n(INFO) ", zjs_get_sec(), zjs_get_ms(), \
              zjs_shorten_filepath(__FILE__), __LINE__, __func__);            \
    ZJS_PRINT
#define ERR_PRINT                                                              \
    ZJS_PRINT("\n%u.%3.3u %s:%d %s():\n(ERROR) ", zjs_get_sec(), zjs_get_ms(), \
              zjs_shorten_filepath(__FILE__), __LINE__, __func__);             \
    ZJS_PRINT
#elif defined(ZJS_VERBOSE) && ZJS_VERBOSE >= 2
// Verbosity level 2 just shows function/line
#define DBG_PRINT                                                       \
    ZJS_PRINT("\n%s:%d %s():\n(INFO) ", zjs_shorten_filepath(__FILE__), \
              __LINE__, __func__);                                      \
    ZJS_PRINT

#define ERR_PRINT                                                        \
    ZJS_PRINT("\n%s:%d %s():\n(ERROR) ", zjs_shorten_filepath(__FILE__), \
              __LINE__, __func__);                                       \
    ZJS_PRINT
#else
// Verbosity level 1 just shows text
#define DBG_PRINT ZJS_PRINT
#define ERR_PRINT ZJS_PRINT
#endif

#else  // !DEBUG_BUILD
#define DBG_PRINT(fmt...) do {} while (0)
#define ERR_PRINT                         \
    ZJS_PRINT("\n%d:(ERROR) ", __LINE__); \
    ZJS_PRINT
#endif  // DEBUG_BUILD

#if defined(DEBUG_BUILD) || defined(USE_ASSERT)
#define ZJS_ASSERT(condition, str)                 \
    if (!(condition)) {                            \
        ERR_PRINT("ASSERTION FAILURE: %s\n", str); \
        do {} while (1);                           \
    }
#else
#define ZJS_ASSERT(condition, str) do {} while (0)
#endif

/**
 * file-specific function tracing for debug purposes
 *
 * Displays short thread id, a file-specific prefix, and the function name,
 *   followed by a formatted string intended to display arguments
 *
 * Use by defining USE_FTRACE and ftrace_prefix string in a file (see zjs_net.c)
 *
 * FTRACE takes format string and arguments like printf
 * FTRACE_JSAPI takes no arguments and expects to run in a ZJS_DECL_FUNC API
 */
#ifdef USE_FTRACE
#define FTRACE                                                   \
    ZJS_PRINT("[%x] %s: %s: ", (u32_t)k_current_get() & 0xffff,  \
              FTRACE_PREFIX, __func__);                          \
    ZJS_PRINT
#define FTRACE_JSAPI                                             \
    ZJS_PRINT("[%x] %s: %s: func = %p, this = %p, argc = %d\n",  \
              (u32_t)k_current_get() & 0xffff, FTRACE_PREFIX,    \
              __func__, (void *)function_obj, (void *)this,      \
              (u32_t)argc)
#else
#define FTRACE(fmt...) do {} while (0)
#define FTRACE_JSAPI do {} while (0)
#endif

/**
 * print function that works only if DEBUG_LOCKS is defined
 */
#ifdef DEBUG_LOCKS
#define LPRINT(str)                                             \
    ZJS_PRINT("[%x] %s: %s\n", (u32_t)k_current_get() & 0xffff, \
              __func__, str)
#else
#define LPRINT(str) do {} while (0)
#endif

// TODO: We should instead have a macro that changes in debug vs. release build,
// to save string space and instead print error codes or something for release.

// this is arbitrary but okay for now; added to avoid plain strlen below
#define MAX_SCRIPT_SIZE 8192

// board-specific
#if defined(CONFIG_BOARD_ARDUINO_101) || defined(CONFIG_BOARD_ARDUINO_101_SSS)
#define ADC_DEVICE_NAME "ADC_0"
#define ADC_BUFFER_SIZE 2
#define AIO_MIN 9
#define AIO_MAX 14
#define AIO_LEN 6
#ifdef CONFIG_BOARD_ARDUINO_101_SSS
#define BMI160_NAME CONFIG_BMI160_NAME
#elif CONFIG_BOARD_ARDUINO_101
#define BMI160_NAME "bmi160"
#endif
#define ACCEL_DEVICE_NAME BMI160_NAME
#define GYRO_DEVICE_NAME BMI160_NAME
#define TEMP_DEVICE_NAME BMI160_NAME
#elif CONFIG_BOARD_FRDM_K64F
#define ADC_BUFFER_SIZE 5
#define ACCEL_DEVICE_NAME CONFIG_FXOS8700_NAME
#define MAGN_DEVICE_NAME CONFIG_FXOS8700_NAME
#endif

// default to blank if not found in board configs
#ifndef ACCEL_DEVICE_NAME
#define ACCEL_DEVICE_NAME ""
#endif

#ifndef GYRO_DEVICE_NAME
#define GYRO_DEVICE_NAME ""
#endif

#ifndef LIGHT_DEVICE_NAME
#define LIGHT_DEVICE_NAME ""
#endif

#ifndef MAGN_DEVICE_NAME
#define MAGN_DEVICE_NAME ""
#endif

#ifndef TEMP_DEVICE_NAME
#define TEMP_DEVICE_NAME ""
#endif

#endif  // __zjs_common_h__
