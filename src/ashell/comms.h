// Copyright (c) 2017, Intel Corporation.

#ifndef __comms_h__
#define __comms_h__

/**
  The mainloop in main.c invokes zjs_ashell_process(), that can operate with
  multiple communications drivers that implement this interface.
 */

// Comms driver interface
typedef struct {
    void (*init)();
    void (*process)(void);
    void (*send)(const char *buf, size_t len);
    void (*close)();
    void (*status)();

    // console functions
    int  (*printch)(int byte);
    void (*print)(const char *buf);
    void (*printf)(const char *format, ...);
    void (*error)(const char *format, ...);
} ashell_comms_t;

// Supported modules, called from ashell.c
ashell_comms_t *comms_uart();
ashell_comms_t *comms_ihex();
ashell_comms_t *comms_webusb();

// Get current comms driver.
ashell_comms_t *ashell_comms();
void ashell_set_comms(ashell_comms_t *driver);

// Functions called from comms modules
extern void ashell_init();
extern void ashell_process();

#endif  // __comms_h__
