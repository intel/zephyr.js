// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_common_h__
#define __zjs_common_h__

// This file includes code common to both X86 and ARC

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

// TODO: We should instead have a macro that changes in debug vs. release build,
// to save string space and instead print error codes or something for release.

#if defined(CONFIG_BOARD_ARDUINO_101) || defined(CONFIG_BOARD_ARDUINO_101_SSS)
#define ARC_AIO_MIN 9
#define ARC_AIO_MAX 14
// ARC_AIO_LEN = ARC_AIO_MAX - ARC_AIO_MIN + 1
#define ARC_AIO_LEN 6
#endif

#endif  // __zjs_common_h__
