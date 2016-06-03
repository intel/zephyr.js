// Copyright (c) 2016, Intel Corporation.
#include <ipm.h>

#define IPM_CHANNEL_X86_TO_ARC                             1
#define IPM_CHANNEL_ARC_TO_X86                             2

#define MSG_ID_AIO                                         1

#define TYPE_AIO_OPEN                                      0
#define TYPE_AIO_OPEN_SUCCESS                              1
#define TYPE_AIO_OPEN_FAIL                                 2

#define TYPE_AIO_PIN_READ                                  3
#define TYPE_AIO_PIN_READ_SUCCESS                          4
#define TYPE_AIO_PIN_READ_FAIL                             5

#define TYPE_AIO_PIN_ABORT                                 6
#define TYPE_AIO_PIN_ABORT_SUCCESS                         7
#define TYPE_AIO_PIN_ABORT_FAIL                            8

#define TYPE_AIO_PIN_CLOSE                                 9
#define TYPE_AIO_PIN_CLOSE_SUCCESS                         10
#define TYPE_AIO_PIN_CLOSE_FAIL                            11

struct zjs_ipm_message {
    bool block;
    uint32_t type;
    uint32_t pin;
    uint32_t value;
};

void zjs_ipm_init();

int zjs_ipm_send(uint32_t id, const void *data, int data_size);

void zjs_ipm_register_callback(uint32_t id, ipm_callback_t cb);
