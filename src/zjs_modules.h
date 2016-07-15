// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_modules_h__
#define __zjs_modules_h__

#include <zephyr.h>

#include "jerry-api.h"

typedef jerry_value_t (*InitCB)();

void zjs_modules_init();
void zjs_modules_add(const char *name, InitCB mod_init);

#endif  // __zjs_modules_h__
