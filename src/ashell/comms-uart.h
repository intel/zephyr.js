// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __comms_uart_h__
#define __comms_uart_h__

#define MAX_LINE 90

/*
 * @brief Interfaces for the different uploaders and process handlers
 *
 * The Application instantiates this with given parameters added
 * using the "comms_uart_set_config" function.
 */

struct terminal_config {
    u32_t (*init)();
    u32_t (*close)();
    u32_t (*process)(const char *buf, u32_t len);
    void  (*error)(u32_t error);
    bool  (*done)();
    void  (*send)(const char *buf, int len);
};

extern struct terminal_config *terminal;

void uart_write_buf(const char *buf, int len);

void comms_print(const char *buf);
void comms_printf(const char *format, ...);

#endif  // __comms_uart_h__
