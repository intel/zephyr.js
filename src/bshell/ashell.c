// Copyright (c) 2016-2017, Intel Corporation.

/**
 * @file
 * @brief Ashell main.
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <malloc.h>
#include <misc/printk.h>
#include <fs.h>
#include <ctype.h>

#include "ashell-def.h"

static const char banner[] = "\r\nZephyr.js shell " __DATE__ " " __TIME__ "\r\n";

ashell_config_t ashell_config = {
    .echo = true,
    .verbose = true,
    .mode = ASHELL_MODE_CHAR,
    .comms = NULL
};

ashell_config_t *ashell_get_config()
{
    return &ashell_config;
}

/**
 * Called from ../main.c mainloop.
 */

void zjs_ashell_init()
{
    if (!ashell_config.comms) {
        ashell_config.comms = comms_uart();  // Change the transport here.
    }

    if (ashell_config.comms->init) {
        ashell_config.comms->init();  // Init transport. Calls ashell_init().
    }
}

/**
 * Initialize ashell. Check boot.cfg and handle bootstrapped JS files.
 */
extern void ashell_bootstrap_js();  // from ashell-cmd.c
extern void ashell_help();
extern void ashell_send_prompt();

void ashell_init()
{
    ashell_bootstrap_js();
    ashell_config.comms->send(banner, sizeof(banner));
    ashell_help();
    ashell_send_prompt();
}

void zjs_ashell_process()
{
    if (ashell_config.comms && ashell_config.comms->process) {
        ashell_config.comms->process();  // Mainloop tick, calls ashell_process().
    }
}

/**
 * Dispatch data received from comms either to line editor or command parser.
 * Called from the comms->process().
 */
extern void ashell_process_char(const char *buf, size_t len);  // ashell-edit.c
extern void ashell_process_line(const char *buf, size_t len);  // ashell-cmd.c
// extern void ashell_process_json(const char *buf, size_t len);  // ashell-json.c

void ashell_process(char *buf, size_t len)
{
    switch (ashell_config.mode) {
        case ASHELL_MODE_CHAR:
            ashell_process_char(buf, len);  // line editor, then parser
            return;
        case ASHELL_MODE_LINE:
            ashell_process_line(buf, len);  // text based command line parser
            return;
        case ASHELL_MODE_JSON:
            // ashell_process_json(buf, len);  // JSON based command parser
            return;
    }
}
