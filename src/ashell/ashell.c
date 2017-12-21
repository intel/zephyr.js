// Copyright (c) 2017, Intel Corporation.

/**
 * @file
 * @brief Ashell main.
 */

#include "ashell.h"

void ide_init()
{
    extern void parser_init();
    parser_init();
}

void zjs_ashell_init()
{
#ifdef ASHELL_IDE_UART
    extern void uart_init();
    uart_init();
#endif
#ifdef ASHELL_IDE_PROTOCOL
    ide_init();
#endif
}

void zjs_ashell_process()
{
#ifdef ASHELL_IDE_UART
    extern void uart_process();
    uart_process();
#endif
}
