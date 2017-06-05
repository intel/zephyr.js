// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_common_h__
#define __zjs_common_h__

// This file includes code common to both X86 and ARC

#include <stdio.h>
#if CONFIG_ARC
#include <misc/printk.h>
#endif

#ifdef ZJS_LINUX_BUILD
typedef int8_t s8_t;
typedef uint8_t u8_t;
typedef int16_t s16_t;
typedef uint16_t u16_t;
typedef int32_t s32_t;
typedef uint32_t u32_t;
typedef int64_t s64_t;
typedef uint64_t u64_t;
#else
#include <zephyr/types.h>
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

#ifdef DEBUG_BUILD

int zjs_get_sec(void);
int zjs_get_ms(void);

#define DBG_PRINT \
    ZJS_PRINT("\n%u.%3.3u %s:%d %s():\n(INFO) ", zjs_get_sec(), zjs_get_ms(), zjs_shorten_filepath(__FILE__), __LINE__, __func__); \
    ZJS_PRINT

#define ERR_PRINT \
    ZJS_PRINT("\n%u.%3.3u %s:%d %s():\n(ERROR) ", zjs_get_sec(), zjs_get_ms(), zjs_shorten_filepath(__FILE__), __LINE__, __func__); \
    ZJS_PRINT

#else
#define DBG_PRINT(fmat ...) do {} while(0);
#define ERR_PRINT \
    ZJS_PRINT("\n%s:%d %s():\n(ERROR) ", zjs_shorten_filepath(__FILE__), __LINE__, __func__); \
    ZJS_PRINT
#endif

// TODO: We should instead have a macro that changes in debug vs. release build,
// to save string space and instead print error codes or something for release.

// this is arbitrary but okay for now; added to avoid plain strlen below
#define MAX_SCRIPT_SIZE 8192

// board-specifc
#if defined(CONFIG_BOARD_ARDUINO_101) || defined(CONFIG_BOARD_ARDUINO_101_SSS)
#define ADC_DEVICE_NAME "ADC_0"
#define ADC_BUFFER_SIZE 2
#define ARC_AIO_MIN 9
#define ARC_AIO_MAX 14
#define ARC_AIO_LEN 6
#ifdef CONFIG_BOARD_ARDUINO_101_SSS
#define BMI160_NAME CONFIG_BMI160_NAME
#elif CONFIG_BOARD_ARDUINO_101
#define BMI160_NAME "bmi160"
#endif
#define ACCEL_DEVICE_NAME BMI160_NAME
#define GYRO_DEVICE_NAME BMI160_NAME
#define TEMP_DEVICE_NAME BMI160_NAME
#elif CONFIG_BOARD_FRDM_K64F
#define ACCEL_DEVICE_NAME CONFIG_FXOS8700_NAME
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

#ifndef TEMP_DEVICE_NAME
#define TEMP_DEVICE_NAME ""
#endif
#endif  // __zjs_common_h__
