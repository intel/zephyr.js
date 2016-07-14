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

#endif  // __zjs_common_h__
