// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <sensor.h>
#include <zephyr.h>

// ZJS includes
#include "arc_common.h"
#include "arc_sensor.h"
#ifdef BUILD_MODULE_SENSOR_LIGHT
#include "arc_aio.h"
#endif

#ifdef CONFIG_BMI160_NAME
#define BMI160_NAME CONFIG_BMI160_NAME
#else
#define BMI160_NAME "bmi160"
#endif

u32_t sensor_poll_freq = 20;        // default polling frequency

#ifdef BUILD_MODULE_SENSOR_LIGHT
static atomic_t pin_values[AIO_LEN] = {};
static atomic_t pin_last_values[AIO_LEN] = {};
static u8_t light_send_updates[AIO_LEN] = {};
#endif

static struct device *bmi160 = NULL;
#ifdef CONFIG_BMI160_TRIGGER
static bool accel_trigger = false;  // trigger mode
static bool gyro_trigger = false;   // trigger mode
#else
static bool accel_poll = false;     // polling mode
static bool gyro_poll = false;      // polling mode
#endif
static bool temp_poll = false;      // polling mode
static double accel_last_value[3];
static double gyro_last_value[3];
static double temp_last_value;

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
#endif  // DEBUG_BUILD

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
    msg.data.sensor.reading = reading;
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

    if (dval[0] != gyro_last_value[0] || dval[1] != gyro_last_value[1] ||
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

    if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val) < 0) {
        ERR_PRINT("failed to read temperature channel\n");
        return;
    }

    dval = convert_sensor_value(&val);
    if (dval != temp_last_value) {
        union sensor_reading reading;
        reading.dval = temp_last_value = dval;
        send_sensor_data(SENSOR_CHAN_AMBIENT_TEMP, reading);
    }

#ifdef DEBUG_BUILD
    char buf[18];

    sensor_value_snprintf(buf, sizeof(buf), &val);
    DBG_PRINT("sending temp: X=%s, Y=%s, Z=%s\n", buf);
#endif
}

#ifdef CONFIG_BMI160_TRIGGER
static void trigger_hdlr(struct device *dev, struct sensor_trigger *trigger)
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
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_CALIB_TARGET,
                        acc_calib) < 0) {
        return -1;
    }

    /*
     * Calibrate gyro. No calibration value needs to be passed to BMI160 as
     * the target on all axis is set internally to 0. This is used just to
     * trigger a gyro calibration.
     */
    if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_CALIB_TARGET,
                        NULL) < 0) {
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

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FULL_SCALE,
                        &attr) < 0) {
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
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH,
                        &attr) < 0) {
        ERR_PRINT("failed set slope threshold\n");
        return -1;
    }

    // set slope duration to 2 consecutive samples
    attr.val1 = 2;
    attr.val2 = 0;
    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_DUR,
                        &attr) < 0) {
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
        } else if (msg->data.sensor.channel == SENSOR_CHAN_AMBIENT_TEMP) {
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
        } else if (msg->data.sensor.channel == SENSOR_CHAN_AMBIENT_TEMP) {
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
static void handle_sensor_light(struct zjs_ipm_message *msg)
{
    u32_t pin;
    u32_t error_code = ERROR_IPM_NONE;

    switch (msg->type) {
    case TYPE_SENSOR_INIT:
        // INIT
        break;
    case TYPE_SENSOR_START:
        pin = msg->data.sensor.pin;
        if (pin < AIO_MIN || pin > AIO_MAX) {
            ERR_PRINT("pin #%u out of range\n", pin);
            error_code = ERROR_IPM_OPERATION_FAILED;
        } else {
            DBG_PRINT("start ambient light %u\n", msg->data.sensor.pin);
            light_send_updates[pin - AIO_MIN] = 1;
        }
        break;
    case TYPE_SENSOR_STOP:
        pin = msg->data.sensor.pin;
        if (pin < AIO_MIN || pin > AIO_MAX) {
            ERR_PRINT("pin #%u out of range\n", pin);
            error_code = ERROR_IPM_OPERATION_FAILED;
        } else {
            DBG_PRINT("stop ambient light %u\n", msg->data.sensor.pin);
            light_send_updates[pin - AIO_MIN] = 0;
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

void arc_handle_sensor(struct zjs_ipm_message *msg)
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
    case SENSOR_CHAN_AMBIENT_TEMP:
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

void arc_fetch_light()
{
    for (int i = 0; i <= 5; i++) {
        if (light_send_updates[i]) {
            u32_t value = arc_pin_read(AIO_MIN + i);
            atomic_set(&pin_values[i], value);
            if (pin_values[i] != pin_last_values[i]) {
                // The formula for converting the analog value to lux is taken
                // from the UPM project:
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
                    reading.dval = 10000.0 /
                                   cube_root(base * base * base * base);
                }
                send_sensor_data(SENSOR_CHAN_LIGHT, reading);
            }
            atomic_set(&pin_last_values[i], value);
        }
    }
}
#endif  // BUILD_MODULE_SENSOR_LIGHT

void arc_fetch_sensor()
{
    // TODO: currently only supports BMI160 temperature
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
