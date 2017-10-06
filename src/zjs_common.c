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

static int start = 0;

int zjs_get_sec(void)
{
    if (!start) {
        start = zjs_port_timer_get_uptime() / 1000;
    }
    return (zjs_port_timer_get_uptime() / 1000) - start;
}

int zjs_get_ms(void)
{
    if (!start) {
        start = zjs_port_timer_get_uptime() / 1000;
    }
    return zjs_port_timer_get_uptime() - (zjs_get_sec() * 1000);
}

#endif  // DEBUG_BUILD
