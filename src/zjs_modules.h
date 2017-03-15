// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_modules_h__
#define __zjs_modules_h__

#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#endif

#include "jerry-api.h"

#define NUM_SERVICE_ROUTINES 3

/**
 * Service routine function type
 *
 * @param handle        Handle that was registered
 *
 * @return              1 if the routine did any processing
 *                      0 if the routine did not process anything
 *
 * Note: The return value is only used for the auto-exit Linux feature. If
 *       0 is returned (and there are no timers or callbacks) AND auto-exit
 *       is enabled, it will cause the program to exit.
 */
typedef uint8_t (*zjs_service_routine)(void *handle);

void zjs_modules_init();
void zjs_modules_cleanup();
void zjs_register_service_routine(void *handle, zjs_service_routine func);
uint8_t zjs_service_routines(void);

#endif  // __zjs_modules_h__
