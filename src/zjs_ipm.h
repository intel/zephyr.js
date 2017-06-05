// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_ipm_h__
#define __zjs_ipm_h__

#include <ipm.h>
#ifdef BUILD_MODULE_SENSOR
#include <sensor.h>
#endif

#define IPM_CHANNEL_X86_TO_ARC                             0x01
#define IPM_CHANNEL_ARC_TO_X86                             0x02

// Message IDs
#define MSG_ID_DONE                                        0x00
#define MSG_ID_AIO                                         0x01
#define MSG_ID_I2C                                         0x02
#define MSG_ID_GLCD                                        0x03
#define MSG_ID_SENSOR                                      0x04
#define MSG_ID_PME                                         0x05

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
#define ERROR_IPM_OPERATION_NOT_ALLOWED                    0x0004

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

// SENSOR
#define TYPE_SENSOR_INIT                                   0x0030
#define TYPE_SENSOR_START                                  0x0031
#define TYPE_SENSOR_STOP                                   0x0032
#define TYPE_SENSOR_EVENT_STATE_CHANGE                     0x0033
#define TYPE_SENSOR_EVENT_READING_CHANGE                   0x0034

// PME
#define TYPE_PME_BEGIN                                     0x0040
#define TYPE_PME_FORGET                                    0x0041
#define TYPE_PME_CONFIGURE                                 0x0042
#define TYPE_PME_LEARN                                     0x0043
#define TYPE_PME_CLASSIFY                                  0x0044
#define TYPE_PME_READ_NEURON                               0x0045
#define TYPE_PME_WRITE_VECTOR                              0x0046
#define TYPE_PME_GET_COMMITED_COUNT                        0x0047
#define TYPE_PME_GET_GLOBAL_CONTEXT                        0x0048
#define TYPE_PME_SET_GLOBAL_CONTEXT                        0x0049
#define TYPE_PME_GET_NEURON_CONTEXT                        0x004A
#define TYPE_PME_SET_NEURON_CONTEXT                        0x004B
#define TYPE_PME_GET_CLASSIFIER_MODE                       0x004C
#define TYPE_PME_SET_CLASSIFIER_MODE                       0x004D
#define TYPE_PME_GET_DISTANCE_MODE                         0x004E
#define TYPE_PME_SET_DISTANCE_MODE                         0x004F
// PME save and restore
#define TYPE_PME_BEGIN_SAVE_MODE                           0x0050
#define TYPE_PME_ITERATE_TO_SAVE                           0x0051
#define TYPE_PME_END_SAVE_MODE                             0x0052
#define TYPE_PME_BEGIN_RESTORE_MODE                        0x0053
#define TYPE_PME_ITERATE_TO_RESTORE                        0x0054
#define TYPE_PME_END_RESTORE_MODE                          0x0055

typedef struct zjs_ipm_message {
    u32_t id;
    u32_t type;
    u32_t flags;
    void *user_data;
    u32_t error_code;

    union {
#ifdef BUILD_MODULE_AIO
        struct aio_data {
            u32_t pin;
            u32_t value;
        } aio;
#endif // AIO

#ifdef BUILD_MODULE_I2C
        struct i2c_data {
            u8_t bus;
            u8_t speed;
            u16_t address;
            u16_t register_addr;
            u8_t *data;
            u32_t length;
        } i2c;
#endif // I2C

#ifdef BUILD_MODULE_GROVE_LCD
        // GROVE_LCD
        struct glcd_data {
            u8_t value;
            u8_t col;
            u8_t row;
            u8_t color_r;
            u8_t color_g;
            u8_t color_b;
            void *buffer;
        } glcd;
#endif // GROVE_LCD

#ifdef BUILD_MODULE_SENSOR
        struct sensor_data {
            enum sensor_channel channel;
            char *controller;
            u32_t pin;
            u32_t frequency;
            union sensor_reading {
                // x y z axis for Accelerometer and Gyroscope
                struct {
                    double x;
                    double y;
                    double z;
                };
                // single value sensors eg. ambient light
                double dval;
            } reading;
        } sensor;
#endif // SENSOR

#ifdef BUILD_MODULE_PME
        struct pme_data {
            u8_t c_mode;
            u8_t d_mode;
            u8_t vector[128];
            u8_t vector_size;
            u16_t category;
            u16_t committed_count;
            u16_t g_context;
            u16_t n_context;
            u16_t aif;
            u16_t min_if;
            u16_t max_if;
            u32_t neuron_id;
        } pme;
#endif // PME
    } data;
} zjs_ipm_message_t;

void zjs_ipm_init();

int zjs_ipm_send(u32_t id, zjs_ipm_message_t *data);

void zjs_ipm_register_callback(u32_t msg_id, ipm_callback_t cb);

void zjs_ipm_free_callbacks();

#endif  // __zjs_ipm_h__
