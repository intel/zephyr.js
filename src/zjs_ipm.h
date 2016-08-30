// Copyright (c) 2016, Intel Corporation.

#ifndef __zjs_ipm_h__
#define __zjs_ipm_h__

#include <ipm.h>

#define IPM_CHANNEL_X86_TO_ARC                             0x01
#define IPM_CHANNEL_ARC_TO_X86                             0x02

#define MSG_ID_AIO                                         0x01
#define MSG_ID_I2C                                         0x02

#define TYPE_AIO_OPEN                                      0x0000
#define TYPE_AIO_OPEN_SUCCESS                              0x0001
#define TYPE_AIO_OPEN_FAIL                                 0x0002

#define TYPE_AIO_PIN_READ                                  0x0003
#define TYPE_AIO_PIN_READ_SUCCESS                          0x0004
#define TYPE_AIO_PIN_READ_FAIL                             0x0005

#define TYPE_AIO_PIN_ABORT                                 0x0006
#define TYPE_AIO_PIN_ABORT_SUCCESS                         0x0007
#define TYPE_AIO_PIN_ABORT_FAIL                            0x0008

#define TYPE_AIO_PIN_CLOSE                                 0x0009
#define TYPE_AIO_PIN_CLOSE_SUCCESS                         0x000A
#define TYPE_AIO_PIN_CLOSE_FAIL                            0x000B

#define TYPE_AIO_PIN_SUBSCRIBE                             0x000C
#define TYPE_AIO_PIN_SUBSCRIBE_SUCCESS                     0x000D
#define TYPE_AIO_PIN_SUBSCRIBE_FAIL                        0x000E

#define TYPE_AIO_PIN_UNSUBSCRIBE                           0x000F
#define TYPE_AIO_PIN_UNSUBSCRIBE_SUCCESS                   0x0010
#define TYPE_AIO_PIN_UNSUBSCRIBE_FAIL                      0x0011

#define TYPE_AIO_PIN_EVENT_VALUE_CHANGE                    0x0012

/***************************** I2C *****************************/

#define TYPE_I2C_OPEN                                      0x0013
#define TYPE_I2C_OPEN_SUCCESS                              0x0014
#define TYPE_I2C_OPEN_FAIL                                 0x0015

#define TYPE_I2C_WRITE                                     0x0016
#define TYPE_I2C_WRITE_SUCCESS                             0x0017
#define TYPE_I2C_WRITE_FAIL                                0x0018

#define TYPE_I2C_WRITE_BIT                                 0x0019
#define TYPE_I2C_WRITE_BIT_SUCCESS                         0x0020
#define TYPE_I2C_WRITE_BIT_FAIL                            0x0021

#define TYPE_I2C_READ                                      0x0022
#define TYPE_I2C_READ_SUCCESS                              0x0023
#define TYPE_I2C_READ_FAIL                                 0x0024

#define TYPE_I2C_TRANSFER                                  0x0025
#define TYPE_I2C_TRANSFER_SUCCESS                          0x0026
#define TYPE_I2C_TRANSFER_FAIL                             0x0027


struct zjs_ipm_message {
    bool block;
    uint32_t type;
    uint32_t pin;
    uint32_t value;
};

struct zjs_i2c_ipm_message {
    bool block;
    uint8_t type;
    uint8_t bus;
    uint8_t speed;
    uint16_t address;
    uint8_t *data;
    uint32_t length;
};

struct zjs_i2c_ipm_message_reply {
    bool block;
    uint8_t type;
};

void zjs_ipm_init();

int zjs_ipm_send(uint32_t id, const void *data, int data_size);

void zjs_ipm_register_callback(uint32_t id, ipm_callback_t cb);

#endif  // __zjs_ipm_h__
