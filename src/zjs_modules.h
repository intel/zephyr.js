// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_modules_h__
#define __zjs_modules_h__

#ifndef ZJS_LINUX_BUILD
#include <zephyr.h>
#endif

#include "jerry-api.h"

#define NUM_SERVICE_ROUTINES 3

typedef jerry_value_t (*initcb_t)();
typedef void (*zjs_service_routine)(void* handle);

void zjs_modules_init();
void zjs_register_service_routine(void* handle, zjs_service_routine func);
void zjs_service_routines(void);

#endif  // __zjs_modules_h__
