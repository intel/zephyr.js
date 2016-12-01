/*
 * Copyright 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __comms_uart_h__
#define __comms_uart_h__

/* Control characters */
/* http://www.physics.udel.edu/~watson/scen103/ascii.html */

#define CTRL_START 0x00
#define CTRL_END   0x1F

/* Escape */
#define ASCII_ESC                0x1b
#define ASCII_DEL                0x7f

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
#define ANSI_ESC           '['
#define ANSI_UP            'A'
#define ANSI_DOWN          'B'
#define ANSI_FORWARD       'C'
#define ANSI_BACKWARD      'D'

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

/**
 * Callback function initialize the process
 */
typedef uint32_t(*process_init_callback_t)();

/**
 * Callback function to pass an error from the transmision
 */
typedef void(*process_error_callback_t)(uint32_t error);

/**
 * Callback function to pass an error from the transmision
 */
typedef uint32_t(*process_data_callback_t)(const char *buf, uint32_t len);

/**
 * Callback to tell when the data transfered is finished or process completed
 */
typedef bool(*process_is_done)();

/**
 * Callback function to pass an error from the transmision
 */
typedef uint32_t(*process_close_callback_t)();

/* Callback to print debug data or state to the user */
typedef void(*process_print_state_t)();

/**
 * Process Status Codes
 */
enum process_status_code
{
    PROCESS_ERROR,        /* Error during upload */
    PROCESS_RESET,        /* Data reset */
    PROCESS_CONNECTED,    /* Client connected */
    PROCESS_UNKNOWN       /* Initial status */
};

/**
 * Callback function with different status
 * When a new usb device is detected or when we are ready to receive data
 */
typedef void(*process_status_callback_t)(enum process_status_code status_code);

/*
 * @brief Interfaces for the different uploaders and process handlers
 */
struct comms_interface_cfg_data
{
    process_init_callback_t init_cb;
    process_close_callback_t close_cb;
    process_data_callback_t process_cb;
    process_error_callback_t error_cb;
    process_is_done is_done;
};

/*
 * @brief UART process data configuration
 *
 * The Application instantiates this with given parameters added
 * using the "comms_uart_set_config" function.
 *
 * This function can be called to swap between different states of the
 * data transactions.
 */

struct comms_cfg_data
{
    /* Callback to be notified on connection status change */
    process_status_callback_t cb_status;
    struct comms_interface_cfg_data interface;

    /* Callback to print debug data or state to the user */
    process_print_state_t print_state;
};

void comms_uart_set_config(struct comms_cfg_data *config);

void comms_print_status();
void comms_clear();

void comms_println(const char *buf);
void comms_write_buf(const char *buf, int len);
void comms_writec(char byte);
void comms_print(const char *buf);
void comms_printf(const char *format, ...);

#endif  // __comms_uart_h__
