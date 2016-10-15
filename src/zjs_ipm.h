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
#define MSG_ID_GLCD                                        0x03

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
#define TYPE_I2C_BURST_READ                                0x0015

// GROVE_LCD
#define TYPE_GLCD_INIT                                     0x0020
#define TYPE_GLCD_PRINT                                    0x0021
#define TYPE_GLCD_CLEAR                                    0x0022
#define TYPE_GLCD_SET_CURSOR_POS                           0X0023
#define TYPE_GLCD_SET_COLOR                                0X0024
#define TYPE_GLCD_SELECT_COLOR                             0X0025
#define TYPE_GLCD_SET_FUNCTION                             0X0026
#define TYPE_GLCD_GET_FUNCTION                             0X0027
#define TYPE_GLCD_GET_DISPLAY_STATE                        0x0028
#define TYPE_GLCD_SET_DISPLAY_STATE                        0x0029
#define TYPE_GLCD_SET_INPUT_STATE                          0x002A
#define TYPE_GLCD_GET_INPUT_STATE                          0x002B


typedef struct zjs_ipm_message {
    uint32_t id;
    uint32_t type;
    uint32_t flags;
    void *user_data;
    uint32_t error_code;

    union {
        // AIO
        struct aio_data {
            uint32_t pin;
            uint32_t value;
        } aio;

        // I2C
        struct i2c_data {
            uint8_t bus;
            uint8_t speed;
            uint16_t address;
            uint16_t register_addr;
            uint8_t *data;
            uint32_t length;
        } i2c;

        // GROVE_LCD
        struct glcd_data {
            uint8_t value;
            uint8_t col;
            uint8_t row;
            uint8_t color_r;
            uint8_t color_g;
            uint8_t color_b;
            void *buffer;
        } glcd;
    } data;
} zjs_ipm_message_t;

void zjs_ipm_init();

int zjs_ipm_send(uint32_t id, zjs_ipm_message_t *data);

void zjs_ipm_register_callback(uint32_t msg_id, ipm_callback_t cb);

void zjs_ipm_free_callbacks();

#endif  // __zjs_ipm_h__
