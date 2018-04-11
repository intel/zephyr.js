// Copyright (c) 2017-2018, Intel Corporation.

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
#include "zjs_util.h"

#include "ide-webusb.h"
extern void __stdout_hook_install(int (*fn)(int));
extern void ide_parse(u8_t *buf, size_t len);

static int webusb_out(int c)
{
    static char buf[20];
    static int size = 0;
    char ch = (char)c;
    buf[size++] = ch;
    if (ch == '\n' || size == 20) {
        webusb_write(buf, size);
        size = 0;
    }
    return 1;
}

int ide_send_buffer(char *buf, size_t len)
{
    return webusb_write(buf, len);
}

void ide_init()
{
    extern void ide_ack();
    webusb_init(ide_parse, ide_ack);

    extern void parser_init();
    parser_init();

    __stdout_hook_install(webusb_out);
}

void ide_process()
{
    webusb_receive_process();
}
