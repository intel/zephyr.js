// Copyright (c) 2017, Intel Corporation.

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

#ifdef JERRY_PORT_ENABLE_JOBQUEUE
#include "../zjs_callbacks.h"
#include "../zjs_util.h"
// The job queue is essentially C callbacks, this is a simple wrapper

typedef struct job_wrapper {
    jerry_job_handler_t handler;
    void *job;
    zjs_callback_id id;
} job_wrapper_t;

static void job_callback(void *h, const void *args)
{
    (void)(args);
    job_wrapper_t *wrapper = (job_wrapper_t *)h;
    wrapper->handler(wrapper->job);
    zjs_remove_callback(wrapper->id);
    zjs_free(wrapper);
}

void jerry_port_jobqueue_enqueue(jerry_job_handler_t handler, void *job_p)
{
    job_wrapper_t *wrapper = zjs_malloc(sizeof(job_wrapper_t));
    wrapper->handler = handler;
    wrapper->job = job_p;
    wrapper->id = zjs_add_c_callback(wrapper, job_callback);
    zjs_signal_callback(wrapper->id, NULL, 0);
}
#endif
