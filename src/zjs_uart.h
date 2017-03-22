// Copyright (c) 2016, Intel Corporation.

#ifndef SRC_ZJS_UART_H_
#define SRC_ZJS_UART_H_

#include "jerryscript.h"

/** Initialize the uart module, or reinitialize after cleanup */
 jerry_value_t zjs_uart_init();

/** Release resources held by the uart module */
void zjs_uart_cleanup();

#endif /* SRC_ZJS_UART_H_ */
