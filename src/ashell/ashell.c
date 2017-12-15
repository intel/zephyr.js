// Copyright (c) 2016-2017, Intel Corporation.

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
    extern void uart_init();
    uart_init();
#if (ASHELL_IDE_PROTOCOL == 1)
    ide_init();
#endif
}

void zjs_ashell_process()
{
    extern void uart_process();
    uart_process();
}
