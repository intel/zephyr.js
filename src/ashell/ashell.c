// Copyright (c) 2017-2018, Intel Corporation.

/**
 * @file
 * @brief Ashell main.
 */

#include "ashell.h"

void zjs_ashell_init()
{
    extern void ide_init();
    ide_init();
}

void zjs_ashell_process()
{
    extern void ide_process();
    ide_process();
}
