// Copyright (c) 2017, Intel Corporation.

#ifndef SRC_ZJS_WEB_SOCKETS_H_
#define SRC_ZJS_WEB_SOCKETS_H_

#include "jerryscript.h"

/* Initialize the net module, or reinitialize after cleanup */
jerry_value_t zjs_ws_init();

/* Release resources held by the net module */
void zjs_ws_cleanup();

#endif /* SRC_ZJS_WEB_SOCKETS_H_ */
