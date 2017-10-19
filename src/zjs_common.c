// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <stdint.h>
#include <string.h>

// ZJS includes
#include "zjs_common.h"

char *zjs_shorten_filepath(char *filepath)
{
    // find end of string
    char *ptr = filepath;
    while (*ptr)
        ptr++;

    // find second to last slash or beginning of string
    int slashcount = 0;
    while (ptr > filepath) {
        if (*ptr == '/')
            slashcount += 1;
        if (slashcount >= 2)
            return ptr + 1;
        ptr--;
    }
    return filepath;
}

#ifdef DEBUG_BUILD
#ifdef ZJS_LINUX_BUILD
#include <time.h>

static u8_t init = 0;
static int seconds = 0;
#else
static int start = 0;
#endif

int zjs_get_sec(void)
{
#ifdef ZJS_LINUX_BUILD
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return now.tv_sec - seconds;
#else
    if (!start) {
        start = zjs_port_timer_get_uptime() / 1000;
    }
    return (zjs_port_timer_get_uptime() / 1000) - start;
#endif
}

int zjs_get_ms(void)
{
#ifdef ZJS_LINUX_BUILD
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return (now.tv_nsec / 1000000);
#else
    if (!start) {
        start = zjs_port_timer_get_uptime() / 1000;
    }
    return zjs_port_timer_get_uptime() - (zjs_get_sec() * 1000);
#endif
}

#endif  // DEBUG_BUILD
