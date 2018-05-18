// Copyright (c) 2017-2018, Intel Corporation.

#include "jerryscript.h"
#include "jerryscript-port.h"

#include <stdarg.h>
#ifdef ZJS_LINUX_BUILD
#include <unistd.h>
#else
#include <zephyr.h>
#endif

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

void jerry_port_console(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void jerry_port_log(jerry_log_level_t level, const char *format, ...)
{
    (void)(level);
    va_list args;
    va_start(args, format);
#ifdef JERRY_DEBUGGER
    char buffer[256];
    int length = 0;
    length = vsnprintf(buffer, 255, format, args);
    buffer[length] = '\0';
    fprintf (stderr, "%s", buffer);
    jerry_char_t *jbuffer = (jerry_char_t *)buffer;
    jerry_debugger_send_output(jbuffer,
                               (jerry_size_t)length,
                               (uint8_t)(level + 2));
#else /* If jerry-debugger isn't defined, libc is turned on */
    vfprintf(stderr, format, args);
#endif /* JERRY_DEBUGGER */
    va_end(args);
}

void jerry_port_sleep (uint32_t sleep_time)
{
#ifdef ZJS_LINUX_BUILD
    usleep ((useconds_t) sleep_time * 1000);
#else
    k_sleep ((useconds_t) sleep_time);
#endif
    (void) sleep_time;
}
