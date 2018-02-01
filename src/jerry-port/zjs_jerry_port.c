// Copyright (c) 2017-2018, Intel Corporation.

#include "jerryscript.h"
#include "jerryscript-port.h"

#include <stdarg.h>

// Stubbed out functions for jerry-port features

bool jerry_port_get_time_zone(jerry_time_zone_t *tz_p)
{
    (void)(tz_p);
    return false;
}

double jerry_port_get_current_time(void)
{
    return 0;
}

void jerry_port_fatal(jerry_fatal_code_t code)
{
    printf("[FATAL]: %u\n", code);
    while (1) {};
}

void jerry_port_console(const char *fmat, ...)
{
    va_list va;
    va_start(va, fmat);
    vprintf(fmat, va);
    va_end(va);
}

void jerry_port_log(jerry_log_level_t level, const char *fmat, ...)
{
    (void)(level);
    va_list va;
    va_start(va, fmat);
    vprintf(fmat, va);
    va_end(va);
}
