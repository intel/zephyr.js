// Copyright (c) 2016-2017, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <device.h>
#include <init.h>
#include <string.h>
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
#ifdef BUILD_MODULE_PME
#include "arc_curie_pme.h"
#endif

#include "zjs_common.h"
#include "zjs_ipm.h"

#define QUEUE_SIZE            10  // max incoming message can handle
#define SLEEP_TICKS            1  // 10ms sleep time in cpu ticks
#define AIO_UPDATE_INTERVAL  200  // 2sec interval in between notifications

#define MAX_I2C_BUS 2

#define MAX_BUFFER_SIZE 256

static struct k_sem arc_sem;
#ifdef CONFIG_IPM
static struct zjs_ipm_message msg_queue[QUEUE_SIZE];
static struct zjs_ipm_message *end_of_queue_ptr = msg_queue + QUEUE_SIZE;
#endif

#ifdef BUILD_MODULE_AIO
#define STACK_SIZE 1024
#define STACK_PRIORITY 7
static char __stack stack[STACK_SIZE];
static struct device *adc_dev = NULL;
static atomic_t pin_enabled[ARC_AIO_LEN] = {};
static atomic_t pin_values[ARC_AIO_LEN] = {};
static atomic_t pin_last_values[ARC_AIO_LEN] = {};
static u8_t seq_buffer[ADC_BUFFER_SIZE];
static void *pin_user_data[ARC_AIO_LEN] = {};
static u8_t pin_send_updates[ARC_AIO_LEN] = {};
#endif

#ifdef BUILD_MODULE_SENSOR_LIGHT
static u8_t light_send_updates[ARC_AIO_LEN] = {};
#endif

#ifdef BUILD_MODULE_I2C
#include "zjs_i2c_handler.h"
#endif

#ifdef BUILD_MODULE_GROVE_LCD
static struct device *glcd = NULL;
static char str[MAX_BUFFER_SIZE];
#endif

#ifdef BUILD_MODULE_SENSOR
static u32_t sensor_poll_freq = 20;  // default poll frequency
static struct device *bmi160 = NULL;
#ifdef CONFIG_BMI160_TRIGGER
static bool accel_trigger = false;      // trigger mode
static bool gyro_trigger = false;       // trigger mode
#else
static bool accel_poll = false;         // polling mode
static bool gyro_poll = false;          // polling mode
#endif
static bool temp_poll = false;          // polling mode
static double accel_last_value[3];
static double gyro_last_value[3];
static double temp_last_value;
#endif

#ifdef BUILD_MODULE_PME
static bool save_mode = false;
static bool restore_mode = false;
#endif

#ifdef CONFIG_IPM
int ipm_send_msg(struct zjs_ipm_message *msg)
{
    msg->flags &= ~MSG_ERROR_FLAG;
    msg->error_code = ERROR_IPM_NONE;
    return zjs_ipm_send(msg->id, msg);
}

static int ipm_send_error(struct zjs_ipm_message *msg,
                          u32_t error_code)
{
    msg->flags |= MSG_ERROR_FLAG;
    msg->error_code = error_code;
    DBG_PRINT("send error %u\n", msg->error_code);
    return zjs_ipm_send(msg->id, msg);
}
#endif

#ifdef BUILD_MODULE_AIO
static u32_t pin_read(u8_t pin)
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
        ERR_PRINT("ADC device not found\n");
        return 0;
    }

    if (adc_read(adc_dev, &entry_table) != 0) {
        ERR_PRINT("couldn't read from pin %d\n", pin);
        return 0;
    }

    // read from buffer, not sure if byte order is important
    u32_t raw_value = (u32_t) seq_buffer[0]
                    | (u32_t) seq_buffer[1] << 8;

    return raw_value;
}
#endif

#ifdef CONFIG_IPM
static void queue_message(struct zjs_ipm_message *incoming_msg)
{
    struct zjs_ipm_message *msg = msg_queue;

    if (!incoming_msg) {
        return;
    }

    k_sem_take(&arc_sem, K_FOREVER);
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
        // running out of space, disregard message
        ERR_PRINT("skipping incoming message\n");
    }
    k_sem_give(&arc_sem);
}

static void ipm_msg_receive_callback(void *context, u32_t id, volatile void *data)
{
    struct zjs_ipm_message *incoming_msg = (struct zjs_ipm_message*)(*(uintptr_t *)data);
    if (incoming_msg) {
        queue_message(incoming_msg);
    } else {
        ERR_PRINT("message is NULL\n");
    }
}
#endif

#ifdef BUILD_MODULE_AIO
static void handle_aio(struct zjs_ipm_message *msg)
{
    u32_t pin = msg->data.aio.pin;
    u32_t reply_value = 0;
    u32_t error_code = ERROR_IPM_NONE;

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        ERR_PRINT("pin #%u out of range\n", pin);
        ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
        return;
    }

    switch (msg->type) {
    case TYPE_AIO_OPEN:
        atomic_set(&pin_enabled[pin - ARC_AIO_MIN], 1);
        break;
    case TYPE_AIO_PIN_READ:
        reply_value = pin_last_values[pin - ARC_AIO_MIN];
        break;
    case TYPE_AIO_PIN_ABORT:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_CLOSE:
        atomic_set(&pin_enabled[pin - ARC_AIO_MIN], 0);
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
        ERR_PRINT("unsupported aio message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    msg->data.aio.pin = pin;
    msg->data.aio.value = reply_value;
    ipm_send_msg(msg);
}

static void process_aio_updates()
{
    for (int i = 0; i <= 5; i++) {
        if (pin_send_updates[i] &&
            pin_values[i] != pin_last_values[i]) {
            // send updates only if value has changed
            // so it doesn't flood the IPM channel
            struct zjs_ipm_message msg;
            msg.id = MSG_ID_AIO;
            msg.type = TYPE_AIO_PIN_EVENT_VALUE_CHANGE;
            msg.flags = 0;
            msg.user_data = pin_user_data[i];
            msg.data.aio.pin = ARC_AIO_MIN + i;
            msg.data.aio.value = pin_values[i];
            ipm_send_msg(&msg);
        }
    }
}

// AIO thread spawned to read from ADC
static void aio_read_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        for (int i = 0; i <= 5; i++) {
            if (pin_enabled[i]) {
                u32_t value = pin_read(ARC_AIO_MIN + i);
                atomic_set(&pin_values[i], value);
                atomic_set(&pin_last_values[i], value);
            }
        }
    }
}
#endif

#ifdef BUILD_MODULE_I2C
static void handle_i2c(struct zjs_ipm_message *msg)
{
    u32_t error_code = ERROR_IPM_NONE;
    u8_t msg_bus = msg->data.i2c.bus;

    switch (msg->type) {
    case TYPE_I2C_OPEN:
        error_code = zjs_i2c_handle_open(msg_bus);
        break;
    case TYPE_I2C_WRITE:
        error_code = zjs_i2c_handle_write(msg_bus,
                                          msg->data.i2c.data,
                                          msg->data.i2c.length,
                                          msg->data.i2c.address);
        break;
    case TYPE_I2C_WRITE_BIT:
        DBG_PRINT("received TYPE_I2C_WRITE_BIT\n");
        break;
    case TYPE_I2C_READ:
        error_code = zjs_i2c_handle_read(msg_bus,
                                         msg->data.i2c.data,
                                         msg->data.i2c.length,
                                         msg->data.i2c.address);
        break;
    case TYPE_I2C_BURST_READ:
        error_code = zjs_i2c_handle_burst_read(msg_bus,
                                               msg->data.i2c.data,
                                               msg->data.i2c.length,
                                               msg->data.i2c.address,
                                               msg->data.i2c.register_addr);
        break;
    case TYPE_I2C_TRANSFER:
        DBG_PRINT("received TYPE_I2C_TRANSFER\n");
        break;
    default:
        ERR_PRINT("unsupported i2c message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    ipm_send_msg(msg);
}
#endif

#ifdef BUILD_MODULE_GROVE_LCD
static void handle_glcd(struct zjs_ipm_message *msg)
{
    char *buffer;
    u8_t r, g, b;
    u32_t error_code = ERROR_IPM_NONE;

    if (msg->type != TYPE_GLCD_INIT && !glcd) {
        ERR_PRINT("Grove LCD device not found\n");
        ipm_send_error(msg, ERROR_IPM_OPERATION_FAILED);
        return;
    }

    switch (msg->type) {
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
            ERR_PRINT("buffer not found\n");
        } else {
            snprintf(str, MAX_BUFFER_SIZE, "%s", buffer);
            glcd_print(glcd, str, strlen(str));
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
        ERR_PRINT("unsupported grove lcd message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    ipm_send_msg(msg);
}
#endif

#ifdef BUILD_MODULE_SENSOR
#ifdef DEBUG_BUILD
static inline int sensor_value_snprintf(char *buf, size_t len,
                                        const struct sensor_value *val)
{
    s32_t val1, val2;

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
}
#endif // DEBUG_BUILD

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
    ipm_send_msg(&msg);
}

#define ABS(x) ((x) >= 0) ? (x) : -(x)

static double convert_sensor_value(const struct sensor_value *val)
{
    // According to documentation, the value is represented as having an
    // integer and a fractional part, and can be obtained using the formula
    // val1 + val2 * 10^(-6).
    return (double)val->val1 + (double)val->val2 * 0.000001;
}

static void process_accel_data(struct device *dev)
{
    struct sensor_value val[3];
    double dval[3];

    if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, val) < 0) {
        ERR_PRINT("failed to read accelerometer channel\n");
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
        accel_last_value[0] = reading.x = dval[0];
        accel_last_value[1] = reading.y = dval[1];
        accel_last_value[2] = reading.z = dval[2];
        send_sensor_data(SENSOR_CHAN_ACCEL_XYZ, reading);
    }

#ifdef DEBUG_BUILD
    char buf_x[18], buf_y[18], buf_z[18];

    sensor_value_snprintf(buf_x, sizeof(buf_x), &val[0]);
    sensor_value_snprintf(buf_y, sizeof(buf_y), &val[1]);
    sensor_value_snprintf(buf_z, sizeof(buf_z), &val[2]);
    DBG_PRINT("sending accel: X=%s, Y=%s, Z=%s\n", buf_x, buf_y, buf_z);
#endif
}

static void process_gyro_data(struct device *dev)
{
    struct sensor_value val[3];
    double dval[3];

    if (sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, val) < 0) {
        ERR_PRINT("failed to read gyroscope channel\n");
        return;
    }

    dval[0] = convert_sensor_value(&val[0]);
    dval[1] = convert_sensor_value(&val[1]);
    dval[2] = convert_sensor_value(&val[2]);

    if (dval[0] != gyro_last_value[0] ||
        dval[1] != gyro_last_value[1] ||
        dval[2] != gyro_last_value[2]) {
        union sensor_reading reading;
        gyro_last_value[0] = reading.x = dval[0];
        gyro_last_value[1] = reading.y = dval[1];
        gyro_last_value[2] = reading.z = dval[2];
        send_sensor_data(SENSOR_CHAN_GYRO_XYZ, reading);
    }

#ifdef DEBUG_BUILD
    char buf_x[18], buf_y[18], buf_z[18];

    sensor_value_snprintf(buf_x, sizeof(buf_x), &val[0]);
    sensor_value_snprintf(buf_y, sizeof(buf_y), &val[1]);
    sensor_value_snprintf(buf_z, sizeof(buf_z), &val[2]);
    DBG_PRINT("sending gyro: X=%s, Y=%s, Z=%s\n", buf_x, buf_y, buf_z);
#endif
}

static void process_temp_data(struct device *dev)
{
    struct sensor_value val;
    double dval;

    if (sensor_channel_get(dev, SENSOR_CHAN_TEMP, &val) < 0) {
        ERR_PRINT("failed to read temperature channel\n");
        return;
    }

    dval = convert_sensor_value(&val);
    if (dval != temp_last_value) {
        union sensor_reading reading;
        reading.dval = temp_last_value = dval;
        send_sensor_data(SENSOR_CHAN_TEMP, reading);
    }

#ifdef DEBUG_BUILD
    char buf[18];

    sensor_value_snprintf(buf, sizeof(buf), &val);
    DBG_PRINT("sending temp: X=%s, Y=%s, Z=%s\n", buf);
#endif
}

#ifdef CONFIG_BMI160_TRIGGER
static void trigger_hdlr(struct device *dev,
                         struct sensor_trigger *trigger)
{
    if (trigger->type != SENSOR_TRIG_DELTA &&
        trigger->type != SENSOR_TRIG_DATA_READY) {
        return;
    }

    if (sensor_sample_fetch(dev) < 0) {
        ERR_PRINT("failed to fetch sensor data\n");
        return;
    }

    if (trigger->chan == SENSOR_CHAN_ACCEL_XYZ) {
        process_accel_data(dev);
    } else if (trigger->chan == SENSOR_CHAN_GYRO_XYZ) {
        process_gyro_data(dev);
    }
}
#endif

/*
 * The values in the following map are the expected values that the
 * accelerometer needs to converge to if the device lies flat on the table. The
 * device has to stay still for about 500ms = 250ms(accel) + 250ms(gyro).
 */
struct sensor_value acc_calib[] = {
    { 0, 0 },      /* X */
    { 0, 0 },      /* Y */
    { 9, 806650 }, /* Z */
};

static int auto_calibration(struct device *dev)
{
    /* calibrate accelerometer */
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                        SENSOR_ATTR_CALIB_TARGET, acc_calib) < 0) {
        return -1;
    }

    /*
     * Calibrate gyro. No calibration value needs to be passed to BMI160 as
     * the target on all axis is set internally to 0. This is used just to
     * trigger a gyro calibration.
     */
    if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
                        SENSOR_ATTR_CALIB_TARGET, NULL) < 0) {
        return -1;
    }

    return 0;
}

#ifdef CONFIG_BMI160_TRIGGER
static int start_accel_trigger(struct device *dev, int freq)
{
    struct sensor_value attr;
    struct sensor_trigger trig;

    // set accelerometer range to +/- 16G. Since the sensor API needs SI
    // units, convert the range to m/s^2.
    sensor_g_to_ms2(16, &attr);

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                        SENSOR_ATTR_FULL_SCALE, &attr) < 0) {
        ERR_PRINT("failed to set accelerometer range\n");
        return -1;
    }

    attr.val1 = freq;
    attr.val2 = 0;

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                        SENSOR_ATTR_SAMPLING_FREQUENCY, &attr) < 0) {
        ERR_PRINT("failed to set accelerometer sampling frequency %d\n", freq);
        return -1;
    }

    // set slope threshold to 0.1G (0.1 * 9.80665 = 4.903325 m/s^2).
    attr.val1 = 0;
    attr.val2 = 980665;
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                        SENSOR_ATTR_SLOPE_TH, &attr) < 0) {
        ERR_PRINT("failed set slope threshold\n");
        return -1;
    }

    // set slope duration to 2 consecutive samples
    attr.val1 = 2;
    attr.val2 = 0;
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
                        SENSOR_ATTR_SLOPE_DUR, &attr) < 0) {
        ERR_PRINT("failed to set slope duration\n");
        return -1;
    }

    // set data ready trigger handler
    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_XYZ;

    if (sensor_trigger_set(dev, &trig, trigger_hdlr) < 0) {
        ERR_PRINT("failed to enable accelerometer trigger\n");
        return -1;
    }

    accel_trigger = true;
    return 0;
}

static int stop_accel_trigger(struct device *dev)
{
    struct sensor_trigger trig;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_ACCEL_XYZ;

    // HACK: Try 50x to stop the sensor
    int count = 0;
    while (sensor_trigger_set(bmi160, &trig, NULL) < 0) {
        if (++count > 50) {
            return -1;
        }
    }

    accel_trigger = false;
    return 0;
}

static int start_gyro_trigger(struct device *dev, int freq)
{
    struct sensor_value attr;
    struct sensor_trigger trig;

    attr.val1 = freq;
    attr.val2 = 0;

    if (sensor_attr_set(bmi160, SENSOR_CHAN_GYRO_XYZ,
                        SENSOR_ATTR_SAMPLING_FREQUENCY, &attr) < 0) {
        ERR_PRINT("failed to set sampling frequency for gyroscope\n");
        return -1;
    }

    // set data ready trigger handler
    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_GYRO_XYZ;

    if (sensor_trigger_set(bmi160, &trig, trigger_hdlr) < 0) {
        ERR_PRINT("failed to enable gyroscope trigger\n");
        return -1;
    }

    gyro_trigger = true;
    return 0;
}

static int stop_gyro_trigger(struct device *dev)
{
    struct sensor_trigger trig;

    trig.type = SENSOR_TRIG_DATA_READY;
    trig.chan = SENSOR_CHAN_GYRO_XYZ;

    // HACK: Try 50x to stop the sensor
    int count = 0;
    while (sensor_trigger_set(bmi160, &trig, NULL) < 0) {
        if (++count > 50) {
            return -1;
        }
    }

    gyro_trigger = false;
    return 0;
}
#endif

static void fetch_sensor()
{
    // ToDo: currently only supports BMI160 temperature
    // add support for other types of sensors
    if (bmi160 && (
#ifndef CONFIG_BMI160_TRIGGER
            accel_poll ||
            gyro_poll ||
#endif
            temp_poll)) {
        if (sensor_sample_fetch(bmi160) < 0) {
            ERR_PRINT("failed to fetch sample from sensor\n");
            return;
        }

#ifndef CONFIG_BMI160_TRIGGER
        if (accel_poll) {
            process_accel_data(bmi160);
        }
        if (gyro_poll) {
            process_gyro_data(bmi160);
        }
#endif
        if (temp_poll) {
            process_temp_data(bmi160);
        }
    }
}

#ifdef BUILD_MODULE_SENSOR_LIGHT
static double cube_root_recursive(double num, double low, double high)
{
    // calculate approximated cube root value of a number recursively
    double margin = 0.0001;
    double mid = (low + high) / 2.0;
    double mid3 = mid * mid * mid;
    double diff = mid3 - num;
    if (ABS(diff) < margin)
        return mid;
    else if (mid3 > num)
        return cube_root_recursive(num, low, mid);
    else
        return cube_root_recursive(num, mid, high);
}

static double cube_root(double num)
{
    return cube_root_recursive(num, 0, num);
}

static void fetch_light()
{
    for (int i = 0; i <= 5; i++) {
        if (light_send_updates[i]) {
            pin_values[i] = pin_read(ARC_AIO_MIN + i);
            if (pin_values[i] != pin_last_values[i]) {
                // The formula for converting the analog value to lux is taken from
                // the UPM project:
                //   https://github.com/intel-iot-devkit/upm/blob/master/src/grove/grove.cxx#L161
                // v = 10000.0/pow(((1023.0-a)*10.0/a)*15.0,4.0/3.0)
                // since pow() is not supported due to missing <math.h>
                // we can calculate using conversion where
                // x^(4/3) is cube root of x^4, where we can
                // multiply base 4 times and take the cube root
                double resistance, base;
                union sensor_reading reading;
                // rescale sample from 12bit (Zephyr) to 10bit (Grove)
                u16_t analog_val = pin_values[i] >> 2;
                if (analog_val > 1015) {
                    // any thing over 1015 will be considered maximum brightness
                    reading.dval = 10000.0;
                } else {
                    resistance = (1023.0 - analog_val) * 10.0 / analog_val;
                    base = resistance * 15.0;
                    reading.dval = 10000.0 / cube_root(base * base * base * base);
                }
                send_sensor_data(SENSOR_CHAN_LIGHT, reading);
            }
            pin_last_values[i] = pin_values[i];
        }
    }
}
#endif // BUILD_MODULE_SENSOR_LIGHT

static void handle_sensor_bmi160(struct zjs_ipm_message *msg)
{
    int freq;
    u32_t error_code = ERROR_IPM_NONE;

    switch (msg->type) {
    case TYPE_SENSOR_INIT:
        if (!bmi160) {
            bmi160 = device_get_binding(BMI160_NAME);

            if (!bmi160) {
                error_code = ERROR_IPM_OPERATION_FAILED;
                ERR_PRINT("failed to initialize BMI160 sensor\n");
            } else {
                if (auto_calibration(bmi160) != 0) {
                    ERR_PRINT("failed to perform auto calibration\n");
                }
                DBG_PRINT("BMI160 sensor initialized\n");
            }
        }
        break;
    case TYPE_SENSOR_START:
        if (!bmi160) {
            error_code = ERROR_IPM_OPERATION_FAILED;
            break;
        }

        freq = msg->data.sensor.frequency;
        if (msg->data.sensor.channel == SENSOR_CHAN_ACCEL_XYZ) {
#ifdef CONFIG_BMI160_TRIGGER
            if (!accel_trigger && start_accel_trigger(bmi160, freq) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
#else
            if (accel_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                sensor_poll_freq = msg->data.sensor.frequency;
                accel_poll = true;
            }
#endif
        } else if (msg->data.sensor.channel == SENSOR_CHAN_GYRO_XYZ) {
#ifdef CONFIG_BMI160_TRIGGER
            if (!gyro_trigger && start_gyro_trigger(bmi160, freq) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
#else
            if (gyro_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                sensor_poll_freq = msg->data.sensor.frequency;
                gyro_poll = true;
            }
#endif
        } else if (msg->data.sensor.channel == SENSOR_CHAN_TEMP) {
            if (temp_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                sensor_poll_freq = msg->data.sensor.frequency;
                temp_poll = true;
            }
        }
        break;
    case TYPE_SENSOR_STOP:
        if (!bmi160) {
            error_code = ERROR_IPM_OPERATION_FAILED;
            break;
        }

        if (msg->data.sensor.channel == SENSOR_CHAN_ACCEL_XYZ) {
#ifdef CONFIG_BMI160_TRIGGER
            if (accel_trigger && stop_accel_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
#else
            if (!accel_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                accel_poll = false;
            }
#endif
        } else if (msg->data.sensor.channel == SENSOR_CHAN_GYRO_XYZ) {
#ifdef CONFIG_BMI160_TRIGGER
            if (gyro_trigger && stop_gyro_trigger(bmi160) != 0) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
#else
            if (!gyro_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                gyro_poll = false;
            }
#endif
        } else if (msg->data.sensor.channel == SENSOR_CHAN_TEMP) {
            if (!temp_poll) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                temp_poll = false;
            }
        }
        break;
    default:
        ERR_PRINT("unsupported sensor message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }
    ipm_send_msg(msg);
}

#ifdef BUILD_MODULE_SENSOR_LIGHT
static void handle_sensor_light(struct zjs_ipm_message* msg)
{
    u32_t pin;
    u32_t error_code = ERROR_IPM_NONE;

    switch (msg->type) {
    case TYPE_SENSOR_INIT:
        // INIT
        break;
    case TYPE_SENSOR_START:
        pin = msg->data.sensor.pin;
        if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
            ERR_PRINT("pin #%u out of range\n", pin);
            error_code = ERROR_IPM_OPERATION_FAILED;
        } else {
            DBG_PRINT("start ambient light %u\n", msg->data.sensor.pin);
            light_send_updates[pin - ARC_AIO_MIN] = 1;
        }
        break;
    case TYPE_SENSOR_STOP:
        pin = msg->data.sensor.pin;
        if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
            ERR_PRINT("pin #%u out of range\n", pin);
            error_code = ERROR_IPM_OPERATION_FAILED;
        } else {
            DBG_PRINT("stop ambient light %u\n", msg->data.sensor.pin);
            light_send_updates[pin - ARC_AIO_MIN] = 0;
        }
        break;
    default:
        ERR_PRINT("unsupported sensor message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }
    ipm_send_msg(msg);
}
#endif

static void handle_sensor(struct zjs_ipm_message *msg)
{
    char *controller = msg->data.sensor.controller;
    switch (msg->data.sensor.channel) {
    case SENSOR_CHAN_ACCEL_XYZ:
    case SENSOR_CHAN_GYRO_XYZ:
        if (!strncmp(controller, BMI160_NAME, 6)) {
            handle_sensor_bmi160(msg);
            return;
        }
        break;
#ifdef BUILD_MODULE_SENSOR_LIGHT
    case SENSOR_CHAN_LIGHT:
        if (!strncmp(controller, ADC_DEVICE_NAME, 5)) {
            handle_sensor_light(msg);
            return;
        }
        break;
#endif
    case SENSOR_CHAN_TEMP:
        if (!strncmp(controller, BMI160_NAME, 6)) {
            handle_sensor_bmi160(msg);
            return;
        }
        break;

    default:
        ERR_PRINT("unsupported sensor channel\n");
        ipm_send_error(msg, ERROR_IPM_NOT_SUPPORTED);
        return;
    }

    ERR_PRINT("unsupported sensor driver\n");
    ipm_send_error(msg, ERROR_IPM_NOT_SUPPORTED);
}
#endif // BUILD_MODULE_SENSOR

#ifdef BUILD_MODULE_PME
static void handle_pme(struct zjs_ipm_message* msg)
{
    u32_t error_code = ERROR_IPM_NONE;
    neuron_data_t data;

    switch (msg->type) {
    case TYPE_PME_BEGIN:
        curie_pme_begin();
        break;
    case TYPE_PME_FORGET:
        curie_pme_forget();
        break;
    case TYPE_PME_CONFIGURE:
        // valid range is 1-127
        if (msg->data.pme.g_context < 1 || msg->data.pme.g_context > 127) {
            ERR_PRINT("context range has to be 1-127\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (msg->data.pme.c_mode != RBF_MODE &&
            msg->data.pme.c_mode != KNN_MODE) {
            ERR_PRINT("invalid classifier mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (msg->data.pme.d_mode != L1_DISTANCE &&
            msg->data.pme.d_mode != LSUP_DISTANCE) {
            ERR_PRINT("invalid distance mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_configure(msg->data.pme.g_context,
                            msg->data.pme.d_mode,
                            msg->data.pme.c_mode,
                            msg->data.pme.min_if,
                            msg->data.pme.max_if);
        break;
    case TYPE_PME_LEARN:
        DBG_PRINT("learning: category=%d is %lu byte vector\n",
                  msg->data.pme.category, msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        curie_pme_learn(msg->data.pme.vector, msg->data.pme.vector_size,
                        msg->data.pme.category);
        DBG_PRINT("\nlearned with %d neruons\n",
                  curie_pme_get_committed_count());
        break;
    case TYPE_PME_CLASSIFY:
        DBG_PRINT("classify: %lu byte vector\n", msg->data.pme.category,
                  msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        msg->data.pme.category = curie_pme_classify(msg->data.pme.vector,
                                                    msg->data.pme.vector_size);
        DBG_PRINT("\ncategory: %d\n", msg->data.pme.category);
        break;
    case TYPE_PME_READ_NEURON:
        DBG_PRINT("read: neuron id=%d\n", msg->data.pme.neuron_id);
        if (msg->data.pme.neuron_id < 1 ||
            msg->data.pme.neuron_id > 128) {
            ERR_PRINT("invalid neuron id, must be between 1 and 128\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_read_neuron(msg->data.pme.neuron_id, &data);
        msg->data.pme.category = data.category;
        msg->data.pme.n_context = data.context;
        msg->data.pme.aif = data.aif;
        msg->data.pme.min_if = data.min_if;
        memcpy(msg->data.pme.vector, data.vector, sizeof(data.vector));
        DBG_PRINT("neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT,
                  data.aif, data.min_if, data.category);
        break;
    case TYPE_PME_WRITE_VECTOR:
        DBG_PRINT("write vector: %lu byte vector\n",
                  msg->data.pme.category, msg->data.pme.vector_size);
        if (msg->data.pme.vector_size > MAX_VECTOR_SIZE) {
            ERR_PRINT("vector cannot be greater than %d\n", MAX_VECTOR_SIZE);
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        if (curie_pme_get_classifier_mode() != KNN_MODE) {
            ERR_PRINT("write vector only supports KNN_MODE\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        for (int i = 0; i < msg->data.pme.vector_size; i++) {
            DBG_PRINT("%d ", msg->data.pme.vector[i]);
        }

        curie_pme_write_vector(msg->data.pme.vector,
                               msg->data.pme.vector_size);
        DBG_PRINT("\nwrote with %d neruons\n", curie_pme_get_committed_count());
        break;
    case TYPE_PME_GET_COMMITED_COUNT:
        msg->data.pme.committed_count = curie_pme_get_committed_count();
        DBG_PRINT("committed count: %d\n", msg->data.pme.committed_count);
        break;
    case TYPE_PME_GET_GLOBAL_CONTEXT:
        msg->data.pme.g_context = curie_pme_get_global_context();
        DBG_PRINT("global context: %d\n", msg->data.pme.g_context);
        break;
    case TYPE_PME_SET_GLOBAL_CONTEXT:
        // valid range is 1-127
        if (msg->data.pme.g_context < 1 || msg->data.pme.g_context > 127) {
            ERR_PRINT("context range has to be 1-127\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_global_context(msg->data.pme.g_context);
        DBG_PRINT("global context: %d\n", msg->data.pme.g_context);
        break;
    case TYPE_PME_GET_NEURON_CONTEXT:
        msg->data.pme.n_context = curie_pme_get_neuron_context();
        DBG_PRINT("neuron context: %d\n", msg->data.pme.n_context);
        break;
    case TYPE_PME_SET_NEURON_CONTEXT:
        // valid range is 1-127
        if (msg->data.pme.n_context < 1 || msg->data.pme.n_context > 127) {
            ERR_PRINT("context range has to be 1-127\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_neuron_context(msg->data.pme.n_context);
        DBG_PRINT("neuron context: %d\n", msg->data.pme.n_context);
        break;
    case TYPE_PME_GET_CLASSIFIER_MODE:
        msg->data.pme.c_mode = curie_pme_get_classifier_mode();
        DBG_PRINT("classifier mode: %d\n", msg->data.pme.c_mode);
        break;
    case TYPE_PME_SET_CLASSIFIER_MODE:
        if (msg->data.pme.c_mode != RBF_MODE &&
            msg->data.pme.c_mode != KNN_MODE) {
            ERR_PRINT("invalid classifier mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_classifier_mode(msg->data.pme.c_mode);
        DBG_PRINT("classifier mode: %d\n", msg->data.pme.c_mode);
        break;
    case TYPE_PME_GET_DISTANCE_MODE:
        msg->data.pme.d_mode = curie_pme_get_distance_mode();
        DBG_PRINT("distance mode: %d\n", msg->data.pme.d_mode);
        break;
    case TYPE_PME_SET_DISTANCE_MODE:
        if (msg->data.pme.d_mode != L1_DISTANCE &&
            msg->data.pme.d_mode != LSUP_DISTANCE) {
            ERR_PRINT("invalid distance mode\n");
            ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
            return;
        }

        curie_pme_set_distance_mode(msg->data.pme.d_mode);
        DBG_PRINT("distance mode: %d\n", msg->data.pme.d_mode);
        break;
    case TYPE_PME_BEGIN_SAVE_MODE:
        curie_pme_begin_save_mode();
        save_mode = true;
        DBG_PRINT("begin save mode\n");
        break;
    case TYPE_PME_ITERATE_TO_SAVE:
        if (!save_mode) {
            ERR_PRINT("not in save mode");
            ipm_send_error(msg, ERROR_IPM_OPERATION_FAILED);
        }

        curie_pme_iterate_neurons_to_save(&data);
        msg->data.pme.category = data.category;
        msg->data.pme.n_context = data.context;
        msg->data.pme.aif = data.aif;
        msg->data.pme.min_if = data.min_if;
        memcpy(msg->data.pme.vector, data.vector, sizeof(data.vector));
        DBG_PRINT("save neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT,
                  data.aif, data.min_if, data.category);
        break;
    case TYPE_PME_END_SAVE_MODE:
        curie_pme_end_save_mode();
        save_mode = false;
        DBG_PRINT("end save mode\n");
        break;
    case TYPE_PME_BEGIN_RESTORE_MODE:
        curie_pme_begin_restore_mode();
        restore_mode = true;
        DBG_PRINT("begin restore mode\n");
        break;
    case TYPE_PME_ITERATE_TO_RESTORE:
        if (!restore_mode) {
            ERR_PRINT("not in restore mode");
            ipm_send_error(msg, ERROR_IPM_OPERATION_FAILED);
        }

        data.category = msg->data.pme.category;
        data.context = msg->data.pme.n_context;
        data.aif = msg->data.pme.aif;
        data.min_if = msg->data.pme.min_if;
        memcpy(data.vector, msg->data.pme.vector, sizeof(msg->data.pme.vector));
        curie_pme_iterate_neurons_to_restore(&data);
        DBG_PRINT("restore neuron: id=%d, CTX=%d, AIF=%d MIF=%d, cat=%d\n",
                  data.context & NCR_ID, data.context & NCR_CONTEXT,
                  data.aif, data.min_if, data.category);

        break;
    case TYPE_PME_END_RESTORE_MODE:
        curie_pme_end_restore_mode();
        restore_mode = false;
        DBG_PRINT("end retore mode\n");
        break;

    default:
        ERR_PRINT("unsupported pme message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    ipm_send_msg(msg);
}
#endif // BUILD_MODULE_PME

#ifdef CONFIG_IPM
static void process_messages()
{
    struct zjs_ipm_message *msg = msg_queue;

    while (msg && msg < end_of_queue_ptr) {
        // loop through all messages and process them
        switch (msg->id) {
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
#ifdef BUILD_MODULE_PME
        case MSG_ID_PME:
            handle_pme(msg);
            break;
#endif
        case MSG_ID_DONE:
            return;
        default:
            ERR_PRINT("unsupported ipm message id: %u, check ARC modules\n",
                      msg->id);
            ipm_send_error(msg, ERROR_IPM_NOT_SUPPORTED);
        }

        msg->id = MSG_ID_DONE;
        msg++;
    }
}
#endif

void main(void)
{
    ZJS_PRINT("Sensor core running ZJS ARC support image\n");

    k_sem_init(&arc_sem, 0, 1);
    k_sem_give(&arc_sem);
#ifdef CONFIG_IPM
    memset(msg_queue, 0, sizeof(struct zjs_ipm_message) * QUEUE_SIZE);

    zjs_ipm_init();
    zjs_ipm_register_callback(-1, ipm_msg_receive_callback); // MSG_ID ignored
#endif

#ifdef BUILD_MODULE_AIO
    adc_dev = device_get_binding(ADC_DEVICE_NAME);
    adc_enable(adc_dev);
    k_thread_spawn(stack, STACK_SIZE, aio_read_thread, NULL, NULL, NULL,
                   STACK_PRIORITY, 0, K_NO_WAIT);
#endif

    int tick_count = 0;
    while (1) {
#ifdef CONFIG_IPM
        process_messages();
#endif
#ifdef BUILD_MODULE_AIO
        if (tick_count % AIO_UPDATE_INTERVAL == 0) {
            process_aio_updates();
        }
#endif
#ifdef BUILD_MODULE_SENSOR
        if (tick_count % (u32_t)(CONFIG_SYS_CLOCK_TICKS_PER_SEC /
                                 sensor_poll_freq) == 0) {
            fetch_sensor();
#ifdef BUILD_MODULE_SENSOR_LIGHT
            fetch_light();
#endif
        }
#endif
        tick_count += SLEEP_TICKS;
        k_sleep(SLEEP_TICKS);
    }

#ifdef BUILD_MODULE_AIO
    adc_disable(adc_dev);
#endif
}
