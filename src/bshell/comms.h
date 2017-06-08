/*
 * Copyright (c) 2016 Intel Corporation
 */

#ifndef __comms_h__
#define __comms_h__

// Comms driver interface.
typedef struct {
    void (*init)();  // invokes ashell_init()
    void (*process)(void); // invokes ashell_process()
    void (*send)(const char *buf, size_t len);
    void (*close)();

    // console functions
    int (*printch)(int byte);
    void (*print)(const char *buf);
    void (*printf)(const char *format, ...);
    void (*error)(const char *format, ...);
} comms_driver_t;


// Supported modules, called from ashell.c
comms_driver_t *comms_uart();  // CDC_ACM and UART-WebUSB (for now)
// comms_driver_t *comms_webusb();  // WebUSB + netbuf transport
// comms_driver_t *comms_ble();  // Bluetooth Smart + netbuf transport

// Get current comms driver.
comms_driver_t *ashell_comms();

// Functions called from comms modules
extern void ashell_init();
extern void ashell_process();

#endif  // __comms_transport_h__
