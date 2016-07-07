// Copyright (c) 2016, Intel Corporation.
#include <ipm.h>

#define IPM_CHANNEL_X86_TO_ARC                             0x01
#define IPM_CHANNEL_ARC_TO_X86                             0x02

#define MSG_ID_AIO                                         0x01

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

struct zjs_ipm_message {
    bool block;
    uint32_t type;
    uint32_t pin;
    uint32_t value;
};

void zjs_ipm_init();

int zjs_ipm_send(uint32_t id, const void *data, int data_size);

void zjs_ipm_register_callback(uint32_t id, ipm_callback_t cb);
