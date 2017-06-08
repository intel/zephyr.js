// Copyright (c) 2016, Intel Corporation.

#ifndef __ashell_def_h__
#define __ashell_def_h__

#define ASHELL_MAX_LINE_LEN  90
#define ASHELL_MAX_CMD_LEN   10

/* Control characters */
/* http://www.physics.udel.edu/~watson/scen103/ascii.html */

#define CTRL_START           0x00
#define CTRL_END             0x1F

#define ASCII_START_OF_TEXT  0x02  /* CTRL-B Start of text */
#define ASCII_END_OF_TEXT    0x03  /* CTRL-C End of text */
#define ASCII_END_OF_TRANS   0x04  /* CTLR-D End of transmission */
#define ASCII_BKSP           0x08
#define ASCII_TAB            '\t'
#define ASCII_CR             '\r'
#define ASCII_NL             '\n'
#define ASCII_CANCEL         0x18  /* CTLR-X */
#define ASCII_SUBSTITUTE     0x1A  /* CTLR-Z */

#define ASCII_ESC            0x1B  /* Escape */
#define ASCII_FILE_SEP       0x1C
#define ASCII_GROUP_SEP      0x1D
#define ASCII_RECORD_SEP     0x1E
#define ASCII_DEL            0x7F

/* ANSI escape sequences */
#define ANSI_ESC             '['
#define ANSI_UP              'A'
#define ANSI_DOWN            'B'
#define ANSI_FORWARD         'C'
#define ANSI_BACKWARD        'D'

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

#include "comms.h"

typedef enum {
    ASHELL_MODE_CHAR,  // for UART, CDC_ACM, WebUSB, with ashell line editing
    ASHELL_MODE_LINE,  // for all transports, with client side line editor
    ASHELL_MODE_JSON   // for all transports, with client side line editor
} ashell_mode_t;

typedef struct {
    bool echo;
    bool verbose;
    ashell_mode_t mode;
    comms_driver_t *comms;
} ashell_config_t;

ashell_config_t *ashell_get_config();

// Access to transport functions.
#define SEND      ashell_get_config()->comms->send
#define PRINTF    ashell_get_config()->comms->printf
#define PRINT     ashell_get_config()->comms->print
#define PRINTCH   ashell_get_config()->comms->printch
#define ERROR     ashell_get_config()->comms->error

#define ECHO_MODE() (ashell_get_config()->echo)
#define SET_ECHO_MODE(b) do { ashell_get_config()->echo = b; } while(0)

#ifndef CONFIG_SHELL_UPLOADER_DEBUG
  #define DBG(...) { ; }
#else
  #define DBG printk
#endif /* CONFIG_SHELL_UPLOADER_DEBUG */

#if 0
    #define INFO       PRINTF
#else
    #define INFO(...)  while(0)
#endif

#endif  // __ashell_def_h__
