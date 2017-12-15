// Copyright (c) 2016-2017, Intel Corporation.

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

// ZJS includes
#include "ashell.h"
#include "ide-comms.h"

// Process a buffer (part of a message) in WebUSB driver context.
void ide_receive(u8_t *buffer, size_t len)
{
    static char rx_buffer[P_SPOOL_SIZE + 1];
    static size_t rx_cursor = 0;
    extern void ide_parse(char *buf, size_t len);
    char *buf = (char *) buffer;

    if (len == 1) {  // character mode; buffer a command
        ide_send_buffer(buf, 1);  // echo
        if (rx_cursor >= P_SPOOL_SIZE || *buf == '\n' || *buf == '\r') {
            //IDE_DBG("\r\nReceived %u chars.\r\n", rx_cursor);
            ide_parse(rx_buffer, rx_cursor);
            rx_cursor = 0;
            memset(rx_buffer, P_SPOOL_SIZE + 1, 0);
        } else {
            rx_buffer[rx_cursor++] = *buf;
            rx_buffer[rx_cursor] = '\0';
        }
    } else {
        //IDE_DBG("\r\nReceived %u bytes.\r\n", len);
        ide_parse(buf, len);
    }
}

// Placeholder for switching WebUSB driver.
#define ASHELL_IDE_UART 1  // Use WebUSB over UART for now.
#if ASHELL_IDE_UART
int ide_send_buffer(char *buf, size_t len)
{
    extern void uart_write_buf(char *buf, int len);
    uart_write_buf(buf, len);
    return len;
}
#else
int ide_send_buffer(char *buf, size_t len)
{
    return webusb_write((u8_t *) buf, len);
}
#endif  // ASHELL_IDE_UART

// Spooled write for webusb-raw is needed, but already there for webusb-uart.
// Temporarily keeping here for testing webusb-raw as well.
static char spool[P_SPOOL_SIZE + 1];
static u32_t spool_cursor = 0;

char *ide_spool_ptr()
{
    return spool + spool_cursor;
}

int ide_spool_space()
{
    return P_SPOOL_SIZE - spool_cursor;
}

void ide_spool_adjust(size_t size)
{
    spool_cursor += size;
}

// Save multiple calls to ide_send, spool the output. Truncate if overflown.
int ide_spool(char *format, ...)
{
    size_t size = 0;
    va_list args;
    va_start(args, format);
    size = vsnprintf(spool + spool_cursor, P_SPOOL_SIZE - spool_cursor,
                     format, args);
    va_end(args);
    spool_cursor += size;
    if (spool_cursor >= P_SPOOL_SIZE) {
        spool_cursor = P_SPOOL_SIZE;
        ide_spool_flush();
    }
    return size;
}

int ide_spool_flush()
{
    if (spool_cursor == 0)
        return 0;
    int size = ide_send_buffer(spool, spool_cursor);
    spool_cursor = 0;
    memset(spool, 0, P_SPOOL_SIZE + 1);
    return size;
}
