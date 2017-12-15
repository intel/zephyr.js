// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __ashell_h__
#define __ashell_h__

// Switches between  console and IDE protocol mode.
#define ASHELL_IDE_PROTOCOL   1

#define ASHELL_IDE_DBG        0

// called from Zephyr.js main.c
void zjs_ashell_init();
void zjs_ashell_process();

#endif  // __ashell_h__
