// Copyright (c) 2017, Intel Corporation.

#ifndef CONFIG_H
#define CONFIG_H

/* Time resolution */
#include <stdint.h>
typedef uint64_t oc_clock_time_t;
#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#endif
#define OC_CLOCK_CONF_TICKS_PER_SECOND (1000)

#define OC_DYNAMIC_ALLOCATION

#define MAX_APP_DATA_SIZE 1024

#endif
