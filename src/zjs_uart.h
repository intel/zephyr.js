#ifndef SRC_ZJS_UART_H_
#define SRC_ZJS_UART_H_

#include "jerry-api.h"

void zjs_service_uart(void);

jerry_object_t* zjs_uart_init(void);

#endif /* SRC_ZJS_UART_H_ */
