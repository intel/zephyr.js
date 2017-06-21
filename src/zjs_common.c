// Copyright (c) 2016-2017, Intel Corporation.

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

static u8_t init = 0;
static int seconds = 0;

#ifdef ZJS_LINUX_BUILD
#include <time.h>

int zjs_get_sec(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return now.tv_sec - seconds;
}

int zjs_get_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return (now.tv_nsec / 1000000);
}

#else  // !ZJS_LINUX_BUILD

#include "zjs_zephyr_port.h"

static zjs_port_timer_t print_timer;
// Millisecond counter to increment
static u32_t milli = 0;

void update_print_timer(void)
{
    if (!init) {
        zjs_port_timer_init(&print_timer);
        zjs_port_timer_start(&print_timer, 1);
        init = 1;
    }
    if (zjs_port_timer_test(&print_timer)) {
        if (milli >= 100) {
            milli = 0;
            seconds++;
        } else {
            // TODO: Find out why incrementing by 1 results in timer being
            //       ~50% too slow. Increasing by 2 works as it did before the
            //       unified kernel.
            milli += 2;
        }
        zjs_port_timer_start(&print_timer, 1);
    }
}

int zjs_get_sec(void)
{
    if (!init) {
        zjs_port_timer_init(&print_timer);
        zjs_port_timer_start(&print_timer, 1);
        init = 1;
    }
    return seconds;
}

int zjs_get_ms(void)
{
    if (!init) {
        zjs_port_timer_init(&print_timer);
        zjs_port_timer_start(&print_timer, 1);
        init = 1;
    }
    return milli;
}

#endif // ZJS_LINUX_BUILD
#endif // DEBUG_BUILD
