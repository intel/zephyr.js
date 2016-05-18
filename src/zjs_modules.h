// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>

#include "jerry-api.h"

typedef jerry_api_object_t *(*InitCB)();

void zjs_modules_init();
void zjs_modules_add (const char *name, InitCB mod_init);
