// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __comms_uart_h__
#define __comms_uart_h__

/* Control characters */
/* http://www.physics.udel.edu/~watson/scen103/ascii.html */

#define CTRL_START 0x00
#define CTRL_END   0x1F

/* Escape */
#define ASCII_ESC                0x1b
#define ASCII_DEL                0x7f
#define ASCII_BKSP               0x08

/* CTLR-X */
#define ASCII_CANCEL             0x18

/* CTRL-B Start of text */
#define ASCII_START_OF_TEXT      0x02

/* CTRL-C End of text */
#define ASCII_END_OF_TEXT        0x03

/* CTLR-Z */
#define ASCII_SUBSTITUTE         0x1A

/* CTLR-D End of transmission */
#define ASCII_END_OF_TRANS       0x04

#define ASCII_CR                 '\r'
#define ASCII_IF                 '\n'
#define ASCII_TAB                '\t'

/* ANSI escape sequences */
#define ANSI_ESC                 '['
#define ANSI_UP                  'A'
#define ANSI_DOWN                'B'
#define ANSI_FORWARD             'C'
#define ANSI_BACKWARD            'D'

/**
 * Ansi helpers
 * https://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-debug-ansi.html
 */

#define ANSI_FG_RED        "\x1b[31m"
#define ANSI_FG_GREEN      "\x1b[32m"
#define ANSI_FG_YELLOW     "\x1b[33m"
#define ANSI_FG_BLUE       "\x1b[34m"
#define ANSI_FG_MAGENTA    "\x1b[35m"
#define ANSI_FG_CYAN       "\x1b[36m"
#define ANSI_FG_WHITE      "\x1b[37m"
#define ANSI_FG_RESTORE    "\x1b[39;0m"
#define ANSI_FG_LIGHT_BLUE "\x1b[34;1m"
#define ANSI_CLEAR         "\x1b[2J\x1b[H"

/**
 * Returns the command prompt of this system
 */
const char *system_get_prompt();

/*
 * @brief Interfaces for the different uploaders and process handlers
 *
 * The Application instantiates this with given parameters added
 * using the "comms_uart_set_config" function.
 */

struct comms_cfg {
    u32_t (*init)();
    u32_t (*close)();
    u32_t (*process)(const char *buf, u32_t len);
    void  (*error)(u32_t error);
    bool  (*done)();
};

void comms_write_buf(const char *buf, int len);
void comms_print(const char *buf);
void comms_printf(const char *format, ...);
void zjs_ashell_process();
void zjs_ashell_init();

#endif  // __comms_uart_h__
