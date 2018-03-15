// Copyright (c) 2017, Intel Corporation.

/**
 * @file
 * @brief Device-side service for Web IDE message exchanges based on WebUSB.
 *
 */

// C includes
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Zephyr includes
#include <atomic.h>
#include <device.h>
#include <init.h>

// ZJS includes
#include "ashell.h"
#include "ide-comms.h"
#include "../zjs_util.h"

#ifdef ASHELL_IDE_UART
  #include "term-uart.h"
#else
  #include "ide-webusb.h"
#endif


static void process_rx_buffer(u8_t *buffer, size_t len)
{
    // TODO: remove this when zephyrjs-ide is switched to IDE protocol.
    static char rx_buffer[P_SPOOL_SIZE + 1];
    static size_t rx_cursor = 0;
    extern void ide_parse(char *buf, size_t len);
    char *buf = (char *) buffer;

    if (len == 1) {  // character mode; buffer a command
        if (rx_cursor >= P_SPOOL_SIZE || *buf == '\n' || *buf == '\r') {
            ide_send_buffer("\r\n", 2);
            ide_parse(rx_buffer, rx_cursor);
            rx_cursor = 0;
            memset(rx_buffer, P_SPOOL_SIZE + 1, 0);
        } else {
            ide_send_buffer(buf, 1);  // echo
            rx_buffer[rx_cursor++] = *buf;
            rx_buffer[rx_cursor] = '\0';
        }
    } else {
        // TODO: keep only this part after cleanup.
        ide_parse(buf, len);
    }
}

int ide_send_buffer(char *buf, size_t len)
{
#ifdef ASHELL_IDE_UART
    uart_write_buf(buf, len);
    return len;
#else
    return webusb_write(buf, len);
#endif
}

// Process a buffer (part of a message) in the webusb_receive_process.
void ide_receive(u8_t *buffer, size_t len)
{
    process_rx_buffer(buffer, len);
}

void ide_init()
{
    webusb_init(ide_receive);

    extern void parser_init();
    parser_init();
}

void ide_process()
{
    webusb_receive_process();
}
