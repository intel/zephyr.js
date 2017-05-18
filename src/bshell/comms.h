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
comms_driver_t *comms_uart();  // CDC_ACM and UART-WebUSB
// comms_driver_t *comms_webusb();  // WebUSB + netbuf transport
// comms_driver_t *comms_ble();  // Bluetooth Smart + netbuf transport

// Get current comms driver.
comms_driver_t *ashell_comms();

// Functions called from comms modules
extern void ashell_init();
extern void ashell_process();


// ==========================================================================

#if 0
// fluffy stuff that is not needed

/**
 * Supported transport types.
 */
typedef enum {
    TRANSPORT_TYPE_NONE,
    TRANSPORT_TYPE_UART,    // Connected via UART + (WebUSB or CDC_ACM)
    TRANSPORT_TYPE_WEBUSB,  // Connected via WebUSB + netbuf
    TRANSPORT_TYPE_BLE      // Connected via Bluetooth Smart
} transport_type_t;

/**
 * Transport states
 */
typedef enum
{
    TRANSPORT_STATE_INIT = 0,     /* Initial status */
    TRANSPORT_STATE_CONNECTED,    /* Client connected */
    TRANSPORT_STATE_ERROR,        /* Error during upload */
    TRANSPORT_STATE_RESET,        /* Data reset */
    TRANSPORT_STATE_IDLE,         /* Data transfer completed */
} transport_state_t;

/**
 * @brief Transport configuration
 */
typedef struct
{
    transport_type_t  type;
    transport_state_t state;
    transport_iface_t *driver;
} comms_transport_t;

#endif
// ==========================================================================

#endif  // __comms_transport_h__
