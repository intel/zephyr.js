// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <string.h>
#include <device.h>
#include <init.h>
#ifdef BUILD_MODULE_AIO
#include <adc.h>
#endif
#ifdef BUILD_MODULE_I2C
#include <i2c.h>
#endif
#ifdef BUILD_MODULE_SENSOR
#include <sensor.h>
#endif
#ifdef BUILD_MODULE_GROVE_LCD
#include <display/grove_lcd.h>
#endif

#include "zjs_common.h"
#include "zjs_ipm.h"

#define QUEUE_SIZE       10  // max incoming message can handle
#define SLEEP_TICKS       1  // 10ms sleep time in cpu ticks
#define UPDATE_INTERVAL 200  // 2sec interval in between notifications

#define ADC_DEVICE_NAME "ADC_0"
#define ADC_BUFFER_SIZE 2

#define MAX_I2C_BUS 1

#define MAX_BUFFER_SIZE 256

static struct k_sem arc_sem;
static struct zjs_ipm_message msg_queue[QUEUE_SIZE];
static struct zjs_ipm_message* end_of_queue_ptr = msg_queue + QUEUE_SIZE;

#ifdef BUILD_MODULE_AIO
static struct device* adc_dev = NULL;
static uint32_t pin_values[ARC_AIO_LEN] = {};
static uint32_t pin_last_values[ARC_AIO_LEN] = {};
static void *pin_user_data[ARC_AIO_LEN] = {};
static uint8_t pin_send_updates[ARC_AIO_LEN] = {};
static uint8_t seq_buffer[ADC_BUFFER_SIZE];
#endif

#ifdef BUILD_MODULE_I2C
static struct device *i2c_device[MAX_I2C_BUS];
#endif

#ifdef BUILD_MODULE_GROVE_LCD
static struct device *glcd = NULL;
static char str[MAX_BUFFER_SIZE];
#endif

#ifdef BUILD_MODULE_SENSOR
static struct device *bmi160 = NULL;
static bool accel_trigger = false;
static bool gyro_trigger = false;
static double accel_last_value[3];
static double gyro_last_value[3];
#endif

// add strnlen() support for security since it is missing
// in Zephyr's minimal libc implementation
size_t strnlen(const char *str, size_t max_len) {
    size_t len;
    for (len = 0; len < max_len; len++) {
        if (!*str)
            break;
        str++;
    }
    return len;
}

static int ipm_send_error_reply(struct zjs_ipm_message *msg, uint32_t error_code) {
    msg->flags |= MSG_ERROR_FLAG;
    msg->error_code = error_code;
    ZJS_PRINT("send error %lu\n", msg->error_code);
    return zjs_ipm_send(msg->id, msg);
}

#ifdef BUILD_MODULE_AIO
static uint32_t pin_read(uint8_t pin)
{
    struct adc_seq_entry entry = {
        .sampling_delay = 12,
        .channel_id = pin,
        .buffer = seq_buffer,
        .buffer_length = ADC_BUFFER_SIZE,
    };

    struct adc_seq_table entry_table = {
        .entries = &entry,
        .num_entries = 1,
    };

    if (!adc_dev) {
       ZJS_PRINT("ADC device not found\n");
       return 0;
    }

    if (adc_read(adc_dev, &entry_table) != 0) {
        ZJS_PRINT("couldn't read from pin %d\n", pin);
        return 0;
    }

    // read from buffer, not sure if byte order is important
    uint32_t raw_value = (uint32_t) seq_buffer[0]
                       | (uint32_t) seq_buffer[1] << 8;

    return raw_value;
}
#endif

static void queue_message(struct zjs_ipm_message* incoming_msg)
{
    struct zjs_ipm_message* msg = msg_queue;

    if (!incoming_msg) {
        return;
    }

    k_sem_take(&arc_sem, TICKS_UNLIMITED);
    while(msg && msg < end_of_queue_ptr) {
       if (msg->id == MSG_ID_DONE) {
           break;
       }
       msg++;
    }

    if (msg != end_of_queue_ptr) {
        // copy the message into our queue to be process in the mainloop
        memcpy(msg, incoming_msg, sizeof(struct zjs_ipm_message));
    } else {
        // running out of spaces, disgard message
        ZJS_PRINT("skipping incoming message\n");
    }
    k_sem_give(&arc_sem);
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    struct zjs_ipm_message *incoming_msg = (struct zjs_ipm_message*)(*(uintptr_t *)data);
    if (incoming_msg) {
        queue_message(incoming_msg);
    } else {
        ZJS_PRINT("error: message is NULL\n");
    }
}

#ifdef BUILD_MODULE_AIO
static void handle_aio(struct zjs_ipm_message* msg)
{
    uint32_t pin = msg->data.aio.pin;
    uint32_t reply_value = 0;
    uint32_t error_code = ERROR_IPM_NONE;

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        ZJS_PRINT("pin #%lu out of range\n", pin);
        ipm_send_error_reply(msg, ERROR_IPM_INVALID_PARAMETER);
        return;
    }

    switch(msg->type) {
    case TYPE_AIO_OPEN:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_READ:
        reply_value = pin_read(pin);
        break;
    case TYPE_AIO_PIN_ABORT:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_CLOSE:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_SUBSCRIBE:
        pin_send_updates[pin - ARC_AIO_MIN] = 1;
        // save user data from subscribe request and return it in change msgs
        pin_user_data[pin - ARC_AIO_MIN] = msg->user_data;
        break;
    case TYPE_AIO_PIN_UNSUBSCRIBE:
        pin_send_updates[pin - ARC_AIO_MIN] = 0;
        pin_user_data[pin - ARC_AIO_MIN] = NULL;
        break;
    default:
        ZJS_PRINT("unsupported aio message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    msg->data.aio.pin = pin;
    msg->data.aio.value = reply_value;
    zjs_ipm_send(msg->id, msg);
}

static void process_aio_updates()
{
    for (int i=0; i<=5; i++) {
        if (pin_send_updates[i]) {
            pin_values[i] = pin_read(ARC_AIO_MIN + i);
            if (pin_values[i] != pin_last_values[i]) {
                // send updates only if value has changed
                // so it doesn't flood the IPM channel
                struct zjs_ipm_message msg;
                msg.id = MSG_ID_AIO;
                msg.type = TYPE_AIO_PIN_EVENT_VALUE_CHANGE;
                msg.flags = 0;
                msg.user_data = pin_user_data[i];
                msg.data.aio.pin = ARC_AIO_MIN+i;
                msg.data.aio.value = pin_values[i];
                zjs_ipm_send(msg.id, &msg);
            }
            pin_last_values[i] = pin_values[i];
        }
    }
}
#endif

#ifdef BUILD_MODULE_I2C
static void handle_i2c(struct zjs_ipm_message* msg)
{
    uint32_t error_code = ERROR_IPM_NONE;
    uint8_t msg_bus = msg->data.i2c.bus;

    switch(msg->type) {
    case TYPE_I2C_OPEN:
        if (msg_bus < MAX_I2C_BUS) {
            char bus[6];
            snprintf(bus, 6, "I2C_%i", msg_bus);
            i2c_device[msg_bus] = device_get_binding(bus);

            if (!i2c_device[msg_bus]) {
                ZJS_PRINT("I2C bus %s not found.\n", bus);
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                /* TODO remove these hard coded numbers
                 * once the config API is made */
                union dev_config cfg;
                cfg.raw = 0;
                cfg.bits.use_10_bit_addr = 0;
                cfg.bits.speed = I2C_SPEED_STANDARD;
                cfg.bits.is_master_device = 1;

                if (i2c_configure(i2c_device[msg_bus], cfg.raw) != 0) {
                    ZJS_PRINT("I2C bus %s configure failed.\n", bus);
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
        } else {
            ZJS_PRINT("I2C bus I2C_%s is not a valid I2C bus.\n", msg_bus);
            error_code = ERROR_IPM_OPERATION_FAILED;
        }
        break;
    case TYPE_I2C_WRITE:
        if (msg_bus < MAX_I2C_BUS) {
            // Write has to come after an Open I2C message
            if (i2c_device[msg_bus]) {
                if (i2c_write(i2c_device[msg_bus],
                              msg->data.i2c.data,
                              msg->data.i2c.length,
                              msg->data.i2c.address) != 0) {
                    ZJS_PRINT("i2c_write failed!\n");
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                ZJS_PRINT("no I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_WRITE_BIT:
        ZJS_PRINT("received TYPE_I2C_WRITE_BIT\n");
        break;
    case TYPE_I2C_READ:
        if (msg_bus < MAX_I2C_BUS) {
            // Read has to come after an Open I2C message
            if (i2c_device[msg_bus]) {

                int reply = i2c_read(i2c_device[msg_bus],
                                     msg->data.i2c.data,
                                     msg->data.i2c.length,
                                     msg->data.i2c.address);

                if (reply < 0) {
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                ZJS_PRINT("No I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_BURST_READ:
        if (msg_bus < MAX_I2C_BUS) {
            // Burst read has to come after an Open I2C message
            if (i2c_device[msg_bus]) {
               int reply = i2c_burst_read(i2c_device[msg_bus],
                                          msg->data.i2c.address,
                                          msg->data.i2c.register_addr,
                                          msg->data.i2c.data,
                                          msg->data.i2c.length);

                if (reply < 0) {
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                ZJS_PRINT("No I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_TRANSFER:
        ZJS_PRINT("received TYPE_I2C_TRANSFER\n");
        break;
    default:
        ZJS_PRINT("unsupported i2c message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    zjs_ipm_send(msg->id, msg);
}
#endif

#ifdef BUILD_MODULE_GROVE_LCD
static void handle_glcd(struct zjs_ipm_message* msg)
{
    char *buffer;
    uint8_t r, g, b;
    uint32_t error_code = ERROR_IPM_NONE;

    if (msg->type != TYPE_GLCD_INIT && !glcd) {
        ZJS_PRINT("Grove LCD device not found.\n");
        ipm_send_error_reply(msg, ERROR_IPM_OPERATION_FAILED);
        return;
    }

    switch(msg->type) {
    case TYPE_GLCD_INIT:
        if (!glcd) {
            /* Initialize the Grove LCD */
            glcd = device_get_binding(GROVE_LCD_NAME);

            if (!glcd) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                DBG_PRINT("Grove LCD initialized\n");
            }
        }
        break;
    case TYPE_GLCD_PRINT:
        /* print to LCD screen */
        buffer = msg->data.glcd.buffer;
        if (!buffer) {
            error_code = ERROR_IPM_INVALID_PARAMETER;
            ZJS_PRINT("buffer not found\n");
        } else {
            snprintf(str, MAX_BUFFER_SIZE, "%s", buffer);
            glcd_print(glcd, str, strnlen(str, MAX_BUFFER_SIZE));
            DBG_PRINT("Grove LCD print: %s\n", str);
        }
        break;
    case TYPE_GLCD_CLEAR:
        glcd_clear(glcd);
        break;
    case TYPE_GLCD_SET_CURSOR_POS:
        glcd_cursor_pos_set(glcd, msg->data.glcd.col, msg->data.glcd.row);
        break;
    case TYPE_GLCD_SET_COLOR:
        r = msg->data.glcd.color_r;
        g = msg->data.glcd.color_g;
        b = msg->data.glcd.color_b;
        glcd_color_set(glcd, r, g, b);
        break;
    case TYPE_GLCD_SELECT_COLOR:
        glcd_color_select(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_SET_FUNCTION:
        glcd_function_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_FUNCTION:
        msg->data.glcd.value = glcd_function_get(glcd);
        break;
    case TYPE_GLCD_SET_DISPLAY_STATE:
        glcd_display_state_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_DISPLAY_STATE:
        msg->data.glcd.value = glcd_display_state_get(glcd);
        break;
    case TYPE_GLCD_SET_INPUT_STATE:
        glcd_input_state_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_INPUT_STATE:
        msg->data.glcd.value = glcd_input_state_get(glcd);
        break;
    default:
        ZJS_PRINT("unsupported grove lcd message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    zjs_ipm_send(msg->id, msg);
}
#endif

#ifdef BUILD_MODULE_SENSOR
#ifdef DEBUG_BUILD
static inline int sensor_value_snprintf(char *buf, size_t len,
                                        const struct sensor_value *val)
{
    int32_t val1, val2;

    switch (val->type) {
    case SENSOR_VALUE_TYPE_INT:
        return snprintf(buf, len, "%d", val->val1);
    case SENSOR_VALUE_TYPE_INT_PLUS_MICRO:
        if (val->val2 == 0) {
            return snprintf(buf, len, "%d", val->val1);
        }

        /* normalize value */
        if (val->val1 < 0 && val->val2 > 0) {
            val1 = val->val1 + 1;
            val2 = val->val2 - 1000000;
        } else {
            val1 = val->val1;
            val2 = val->val2;
        }

        /* print value to buffer */
        if (val1 > 0 || (val1 == 0 && val2 > 0)) {
                return snprintf(buf, len, "%d.%06d", val1, val2);
        } else if (val1 == 0 && val2 < 0) {
                return snprintf(buf, len, "-0.%06d", -val2);
        } else {
                return snprintf(buf, len, "%d.%06d", val1, -val2);
        }
    case SENSOR_VALUE_TYPE_DOUBLE:
        return snprintf(buf, len, "%f", val->dval);
    default:
        return 0;
    }
}
#endif

static void send_sensor_data(enum sensor_channel channel,
                             union sensor_reading reading)
{
    struct zjs_ipm_message msg;
    msg.id = MSG_ID_SENSOR;
    msg.type = TYPE_SENSOR_EVENT_READING_CHANGE;
    msg.flags = 0;
    msg.user_data = NULL;
    msg.error_code = ERROR_IPM_NONE;
    msg.data.sensor.channel = channel;
    memcpy(&msg.data.sensor.reading, &reading, sizeof(union sensor_reading));
    zjs_ipm_send(MSG_ID_SENSOR, &msg);
}

#define ABS(x) ((x) >= 0) ? (x) : -(x)

static double convert_sensor_value(const struct sensor_value *val)
{
    int32_t val1, val2;
    double result = 0;

    switch (val->type) {
    case SENSOR_VALUE_TYPE_INT:
        result = val->val1;
        break;
    case SENSOR_VALUE_TYPE_INT_PLUS_MICRO:
        if (val->val2 == 0) {
            result = (double)val->val1;
            break;
        }

        /* normalize value */
        if (val->val1 < 0 && val->val2 > 0) {
            val1 = val->val1 + 1;
            val2 = val->val2 - 1000000;
        } else {
            val1 = val->val1;
            val2 = val->val2;
        }

        if (val1 > 0 || (val1 == 0 && val2 > 0)) {
            result = val1 + (double)val2 * 0.000001;
        } else if (val1 == 0 && val2 < 0) {
            result = (double)val2 * (-0.000001);
        } else {
            result = val1 + (double)val2 * (-0.000001);
        }
        break;
    case SENSOR_VALUE_TYPE_DOUBLE:
        result = val->dval;
        break;
    default:
        ZJS_PRINT("convert_sensor_value: invalid type %d\n", val->type);
        return 0;
    }

    return result;
}

static void process_accel_data(struct device *dev)
{
    struct sensor_value val[3];
    double dval[3];

    if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_ANY, val) < 0) {
        ZJS_PRINT("failed to read accelerometer channels\n");
        return;
    }

    dval[0] = convert_sensor_value(&val[0]);
    dval[1] = convert_sensor_value(&val[1]);
    dval[2] = convert_sensor_value(&val[2]);

    // set slope threshold to 0.1G (0.1 * 9.80665 = 4.903325 m/s^2)
    double threshold = 0.980665;
    if (ABS(dval[0] - accel_last_value[0]) > threshold ||
        ABS(dval[1] - accel_last_value[1]) > threshold ||
        ABS(dval[2] - accel_last_value[2]) > threshold) {
        union sensor_reading reading;
        reading.x = dval[0];
        reading.y = dval[1];
        reading.z = dval[2];
        send_sensor_data(SENSOR_CHAN_ACCEL_ANY, reading);
    }

#ifdef DEBUG_BUILD
    char buf_x[18], buf_y[18], buf_z[18];

    sensor_value_snprintf(buf_x, sizeof(buf_x), &val[0]);
    sensor_value_snprintf(buf_y, sizeof(buf_y), &val[1]);
    sensor_value_snprintf(buf_z, sizeof(buf_z), &val[2]);
    ZJS_PRINT("sending accel: X=%s, Y=%s, Z=%s\n", buf_x, buf_y, buf_z);
#endif
}

static void process_gyro_data(struct device *dev)
{
    struct sensor_value val[3];
    double dval[3];

    if (sensor_channel_get(dev, SENSOR_CHAN_GYRO_ANY, val) < 0) {
        ZJS_PRINT("failed to read gyroscope channels\n");
        return;
    }

    dval[0] = convert_sensor_value(&val[0]);
    dval[1] = convert_sensor_value(&val[1]);
    dval[2] = convert_sensor_value(&val[2]);

    if (dval[0] != gyro_last_value[0] ||
        dval[1] != gyro_last_value[1] ||
        dval[2] != gyro_last_value[2]) {
        union sensor_reading reading;
        reading.x = dval[0];
        reading.y = dval[1];
        reading.z = dval[2];
        send_sensor_data(SENSOR_CHAN_GYRO_ANY, reading);
    }

#ifdef DEBUG_BUILD
    char buf_x[18], buf_y[18], buf_z[18];

    sensor_value_snprintf(buf_x, sizeof(buf_x), &val[0]);
    sensor_value_snprintf(buf_y, sizeof(buf_y), &val[1]);
    sensor_value_snprintf(buf_z, sizeof(buf_z), &val[2]);
    ZJS_PRINT("sending gyro: X=%s, Y=%s, Z=%s\n", buf_x, buf_y, buf_z);
#endif
}

static void trigger_hdlr(struct device *dev,
                         struct sensor_trigger *trigger)
{
    if (trigger->type != SENSOR_TRIG_DELTA &&
        trigger->type != SENSOR_TRIG_DATA_READY) {
        return;
    }

    if (sensor_sample_fetch(dev) < 0) {
        ZJS_PRINT("failed to fetch sensor data\n");
        return;
    }

    if (trigger->chan == SENSOR_CHAN_ACCEL_ANY) {
        process_accel_data(dev);
    } else if (trigger->chan == SENSOR_CHAN_GYRO_ANY) {
        process_gyro_data(dev);
    }
}

/*
 * The values in the following map are the expected values that the
 * accelerometer needs to converge to if the device lies flat on the table. The
 * device has to stay still for about 500ms = 250ms(accel) + 250ms(gyro).
 */
struct sensor_value acc_calib[] = {
    {SENSOR_VALUE_TYPE_INT_PLUS_MICRO, { {0, 0} } },      /* X */
    {SENSOR_VALUE_TYPE_INT_PLUS_MICRO, { {0, 0} } },      /* Y */
    {SENSOR_VALUE_TYPE_INT_PLUS_MICRO, { {9, 806650} } }, /* Z */
};

static int auto_calibration(struct device *dev)
{
    /* calibrate accelerometer */
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_ANY,
                        SENSOR_ATTR_CALIB_TARGET, acc_calib) < 0) {
        return -1;
    }

    /*
     * Calibrate gyro. No calibration value needs to be passed to BMI160 as
     * the target on all axis is set internally to 0. This is used just to
     * trigger a gyro calibration.
     */
    if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_ANY,
                        SENSOR_ATTR_CALIB_TARGET, NULL) < 0) {
        return -1;
    }

    return 0;
}

static int start_accel_trigger(struct device *dev)
{
    struct sensor_value attr;
    struct sensor_trigger trig;

    // set accelerometer range to +/- 16G. Since the sensor API needs SI
    // units, convert the range to m/s^2.
    sensor_g_to_ms2(16, &attr);

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_ANY,
                        SENSOR_ATTR_FULL_SCALE, &attr) < 0) {
        ZJS_PRINT("failed to set accelerometer range\n");
        return -1;
    }

    // set sampling frequency to 50Hz for accelerometer
    attr.type = SENSOR_VALUE_TYPE_INT_PLUS_MICRO;
    attr.val1 = 50;
    attr.val2 = 0;

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_ANY,
                        SENSOR_ATTR_SAMPLING_FREQUENCY, &attr) < 0) {
        ZJS_PRINT("failed to set accelerometer sampling frequency\n");
        return -1;
    }

    // set slope threshold to 0.1G (0.1 * 9.80665 = 4.903325 m/s^2).
    attr.type = SENSOR_VALUE_TYPE_INT_PLUS_MICRO;
    attr.val1 = 0;
    attr.val2 = 980665;
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_ANY,
                        SENSOR_ATTR_SLOPE_TH, &attr) < 0) {
        ZJS_PRINT("failed set slope threshold\n");
        return -1;
    }

    // set slope duration to 2 consecutive samples
    attr.type = SENSOR_VALUE_TYPE_INT;
    attr.val1 = 2;
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_ANY,
                        SENSOR_ATTR_SLOPE_DUR, &attr) < 0) {
        ZJS_PRINT("failed to set slope duration\n");
        return -1;
    }

    // set data ready trigger handler
    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_ANY;

    if (sensor_trigger_set(dev, &trig, trigger_hdlr) < 0) {
        ZJS_PRINT("failed to enable accelerometer trigger\n");
        return -1;
    }

    accel_trigger = true;
    return 0;
}

static int stop_accel_trigger(struct device *dev)
{
    struct sensor_trigger trig;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_ANY;

    if (sensor_trigger_set(bmi160, &trig, NULL) < 0) {
        ZJS_PRINT("failed to disable accelerometer trigger\n");
        return -1;
    }

    accel_trigger = false;
    return 0;
}

static int start_gyro_trigger(struct device *dev)
{
    struct sensor_value attr;
    struct sensor_trigger trig;

    // set sampling frequency to 50Hz for gyroscope
    attr.type = SENSOR_VALUE_TYPE_INT_PLUS_MICRO;
    attr.val1 = 50;
    attr.val2 = 0;

    if (sensor_attr_set(bmi160, SENSOR_CHAN_GYRO_ANY,
                        SENSOR_ATTR_SAMPLING_FREQUENCY, &attr) < 0) {
        ZJS_PRINT("failed to set sampling frequency for gyroscope.\n");
        return -1;
    }

    // set data ready trigger handler
    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_GYRO_ANY;

    if (sensor_trigger_set(bmi160, &trig, trigger_hdlr) < 0) {
        ZJS_PRINT("failed to enable gyroscope trigger.\n");
        return -1;
    }

    gyro_trigger = true;
    return 0;
}

static int stop_gyro_trigger(struct device *dev)
{
    struct sensor_trigger trig;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_GYRO_ANY;

    if (sensor_trigger_set(bmi160, &trig, NULL) < 0) {
        ZJS_PRINT("failed to disable gyroscope trigger\n");
        return -1;
    }

    gyro_trigger = false;
    return 0;
}

static void handle_sensor(struct zjs_ipm_message* msg)
{
    uint32_t error_code = ERROR_IPM_NONE;

    if (msg->type != TYPE_SENSOR_INIT && !bmi160) {
        ZJS_PRINT("BMI160 sensor not found.\n");
        ipm_send_error_reply(msg, ERROR_IPM_OPERATION_FAILED);
        return;
    }

    switch(msg->type) {
    case TYPE_SENSOR_INIT:
        if (!bmi160) {
            bmi160 = device_get_binding("bmi160");

            if (!bmi160) {
                error_code = ERROR_IPM_OPERATION_FAILED;
                ZJS_PRINT("failed to initialize BMI160 sensor\n");
            } else {
                if (auto_calibration(bmi160)) {
                    ZJS_PRINT("failed to perform auto calibration\n");
                }
                DBG_PRINT("BMI160 sensor initialized\n");
            }
        }
        break;
    case TYPE_SENSOR_START:
        if (msg->data.sensor.channel == SENSOR_CHAN_ACCEL_ANY) {
            if (!accel_trigger && start_accel_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        } else if (msg->data.sensor.channel == SENSOR_CHAN_GYRO_ANY) {
            if (!gyro_trigger && start_gyro_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        } else {
            ZJS_PRINT("invalid sensor channel\n");
            error_code = ERROR_IPM_NOT_SUPPORTED;
        }
        break;
    case TYPE_SENSOR_STOP:
        if (msg->data.sensor.channel == SENSOR_CHAN_ACCEL_ANY) {
            if (accel_trigger && stop_accel_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        } else if (msg->data.sensor.channel == SENSOR_CHAN_GYRO_ANY) {
            if (gyro_trigger && stop_gyro_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        } else {
            ZJS_PRINT("invalid sensor channel\n");
            error_code = ERROR_IPM_NOT_SUPPORTED;
        }
        break;
    default:
        ZJS_PRINT("unsupported sensor message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    zjs_ipm_send(msg->id, msg);
}
#endif

static void process_messages()
{
    struct zjs_ipm_message* msg = msg_queue;

    while (msg && msg < end_of_queue_ptr) {
        // loop through all messages and process them
       switch(msg->id) {
#ifdef BUILD_MODULE_AIO
       case MSG_ID_AIO:
           handle_aio(msg);
           break;
#endif
#ifdef BUILD_MODULE_I2C
       case MSG_ID_I2C:
           handle_i2c(msg);
           break;
#endif
#ifdef BUILD_MODULE_GROVE_LCD
       case MSG_ID_GLCD:
           handle_glcd(msg);
           break;
#endif
#ifdef BUILD_MODULE_SENSOR
       case MSG_ID_SENSOR:
           handle_sensor(msg);
           break;
#endif
       case MSG_ID_DONE:
           return;
       default:
           ZJS_PRINT("unsupported ipm message id: %lu, check ARC modules\n", msg->id);
           ipm_send_error_reply(msg, ERROR_IPM_NOT_SUPPORTED);
       }

       msg->id = MSG_ID_DONE;
       msg++;
    }
}

void main(void)
{
    ZJS_PRINT("Sensor core running ZJS ARC support image\n");

    k_sem_init(&arc_sem, 0, 1);
    k_sem_give(&arc_sem);

    memset(msg_queue, 0, sizeof(struct zjs_ipm_message) * QUEUE_SIZE);

    zjs_ipm_init();
    zjs_ipm_register_callback(-1, ipm_msg_receive_callback); // MSG_ID ignored

#ifdef BUILD_MODULE_AIO
    adc_dev = device_get_binding(ADC_DEVICE_NAME);
    adc_enable(adc_dev);

    int tick_count = 0;
#endif

    while (1) {
        process_messages();
#ifdef BUILD_MODULE_AIO
        if (tick_count >= UPDATE_INTERVAL) {
            process_aio_updates();
            tick_count = 0;
        }
        tick_count += SLEEP_TICKS;
#endif
        k_sleep(SLEEP_TICKS);
    }

#ifdef BUILD_MODULE_AIO
    adc_disable(adc_dev);
#endif
}
