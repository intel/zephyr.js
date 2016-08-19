// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_ipm_h__
#define __zjs_ipm_h__

#include <ipm.h>

#define IPM_CHANNEL_X86_TO_ARC                             0x01
#define IPM_CHANNEL_ARC_TO_X86                             0x02

// Message IDs
#define MSG_ID_DONE                                        0x00
#define MSG_ID_AIO                                         0x01
#define MSG_ID_I2C                                         0x02

// Message flags
enum {
     MSG_SYNC_FLAG =                                       0x01,
     MSG_ERROR_FLAG =                                      0x02,
     MSG_SAFE_TO_FREE_FLAG =                               0x04
};

// Error Codes
#define ERROR_IPM_NONE                                     0x0000
#define ERROR_IPM_NOT_SUPPORTED                            0x0001
#define ERROR_IPM_INVALID_PARAMETER                        0x0002
#define ERROR_IPM_OPERATION_FAILED                         0x0003

// Message Types

// AIO
#define TYPE_AIO_OPEN                                      0x0000
#define TYPE_AIO_PIN_READ                                  0x0001
#define TYPE_AIO_PIN_ABORT                                 0x0002
#define TYPE_AIO_PIN_CLOSE                                 0x0003
#define TYPE_AIO_PIN_SUBSCRIBE                             0x0004
#define TYPE_AIO_PIN_UNSUBSCRIBE                           0x0005
#define TYPE_AIO_PIN_EVENT_VALUE_CHANGE                    0x0006

// I2C
#define TYPE_I2C_OPEN                                      0x0010
#define TYPE_I2C_WRITE                                     0x0011
#define TYPE_I2C_WRITE_BIT                                 0x0012
#define TYPE_I2C_READ                                      0x0013
#define TYPE_I2C_TRANSFER                                  0x0014


struct zjs_ipm_message {
    uint32_t id;                                           // message id
    uint32_t type;                                         // message type
    uint32_t flags;                                        // flags
    void* user_data;                                       // user data
    uint32_t error_code;

    union {
        // AIO
        struct aio_data {
            uint32_t pin;
            uint32_t value;
        } aio;

        // I2C
        struct i2c_data{
            uint8_t bus;
            uint8_t speed;
            uint16_t address;
            uint8_t *data;
            uint32_t length;
        } i2c;
    } data;                                                // message data
};

void zjs_ipm_init();

int zjs_ipm_send(uint32_t id, struct zjs_ipm_message *data);

void zjs_ipm_register_callback(ipm_callback_t cb);

#endif  // __zjs_ipm_h__
