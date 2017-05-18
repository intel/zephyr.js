// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_common_h__
#define __zjs_common_h__

// This file includes code common to both X86 and ARC

#include <stdio.h>
#if CONFIG_ARC
#include <misc/printk.h>
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

#if defined(CONFIG_BOARD_ARDUINO_101) || defined(CONFIG_BOARD_ARDUINO_101_SSS)
#define ADC_DEVICE_NAME "ADC_0"
#define ADC_BUFFER_SIZE 2
#define ARC_AIO_MIN 9
#define ARC_AIO_MAX 14
#define ARC_AIO_LEN 6
#define BMI160_DEVICE_NAME "bmi160"
#endif

#endif  // __zjs_common_h__
