// Copyright (c) 2016, Intel Corporation.

#ifndef ZJS_SCRIPT_H_
#define ZJS_SCRIPT_H_

#include "zjs_util.h"
#include "zjs_common.h"

#include <stdlib.h>

uint8_t zjs_read_script(char *name, char **script, uint32_t *length);

void zjs_free_script(char *script);

#endif /* ZJS_SCRIPT_H_ */
