// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_SENSOR
#ifndef QEMU_BUILD
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>

// ZJS includes
#include "zjs_sensor.h"
#include "zjs_callbacks.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_SENSOR_TIMEOUT_TICKS                      500

static struct nano_sem sensor_sem;

enum sensor_state {
    SENSOR_STATE_IDLE,
    SENSOR_STATE_ACTIVATING,
    SENSOR_STATE_ACTIVATED,
    SENSOR_STATE_ERRORED,
};

typedef struct sensor_handle {
    int32_t id;
    enum sensor_channel channel;
    enum sensor_state state;
    double sensor_value[3];
    jerry_value_t sensor_obj;
    struct sensor_handle *next;
} sensor_handle_t;

static sensor_handle_t *accel_handle = NULL;
static sensor_handle_t *gyro_handle = NULL;

static sensor_handle_t *zjs_sensor_alloc_handle(enum sensor_channel channel)
{
    size_t size = sizeof(sensor_handle_t);
    sensor_handle_t *handle = zjs_malloc(size);
    if (handle) {
        memset(handle, 0, size);
    }

    // append to the list
    sensor_handle_t **head = NULL;
    if (channel == SENSOR_CHAN_ACCEL_ANY) {
        head = &accel_handle;
    }
    else if (channel == SENSOR_CHAN_GYRO_ANY) {
        head = &gyro_handle;
    } else {
        PRINT("zjs_sensor_alloc_handle: invalid channel\n");
        zjs_free(handle);
        return NULL;
    }

    while (*head != NULL) {
        head = &(*head)->next;
    }
    *head = handle;

    return handle;
}

static void zjs_sensor_free_handles(sensor_handle_t *handle)
{
    sensor_handle_t *tmp;
    while (handle != NULL) {
        tmp = handle;
        handle = handle->next;
        jerry_release_value(tmp->sensor_obj);
        zjs_free(tmp);
    }
}

static bool zjs_sensor_ipm_send_sync(zjs_ipm_message_t* send,
                                     zjs_ipm_message_t* result)
{
    send->id = MSG_ID_SENSOR;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_SENSOR, send) != 0) {
        PRINT("zjs_sensor_ipm_send_sync: failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (!nano_sem_take(&sensor_sem, ZJS_SENSOR_TIMEOUT_TICKS)) {
        PRINT("zjs_sensor_ipm_send_sync: FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static int zjs_sensor_call_remote_function(zjs_ipm_message_t* send)
{
    zjs_ipm_message_t reply;
    if (!zjs_sensor_ipm_send_sync(send, &reply)) {
        return zjs_error("zjs_sensor_call_remote_function: ipm message failed or timed out!");
    }
    if (reply.error_code != ERROR_IPM_NONE) {
        PRINT("zjs_sensor_call_remote_function: error code: %lu\n",
              reply.error_code);
    }
    return reply.error_code;
}

static enum sensor_state zjs_sensor_get_state(jerry_value_t obj)
{
    const int BUFLEN = 20;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "state", buffer, BUFLEN)) {
        if (!strcmp(buffer, "idle"))
            return SENSOR_STATE_IDLE;
        if (!strcmp(buffer, "activating"))
            return SENSOR_STATE_ACTIVATING;
        if (!strcmp(buffer, "activated"))
            return SENSOR_STATE_ACTIVATED;
        if (!strcmp(buffer, "errored"))
            return SENSOR_STATE_ERRORED;
    }

    PRINT("zjs_sensor_get_state: state not set\n");
    jerry_value_t state = jerry_create_string("errored");
    zjs_set_property(obj, "state", state);
    jerry_release_value(state);
    return SENSOR_STATE_ERRORED;
}

static void zjs_sensor_set_state(jerry_value_t obj, enum sensor_state state)
{
    enum sensor_state old_state = zjs_sensor_get_state(obj);
    if (old_state == state) {
        return;
    }

    // update state property and trigger onstatechange event
    jerry_value_t new_state;
    switch(state) {
    case SENSOR_STATE_IDLE:
        new_state = jerry_create_string("idle");
        break;
    case SENSOR_STATE_ACTIVATING:
        new_state = jerry_create_string("activating");
        break;
    case SENSOR_STATE_ACTIVATED:
        new_state = jerry_create_string("activated");
        break;
    case SENSOR_STATE_ERRORED:
        new_state = jerry_create_string("errored");
        break;
    }
    zjs_set_property(obj, "state", new_state);

    jerry_value_t func = zjs_get_property(obj, "onstatechange");
    if (jerry_value_is_function(func)) {
        // if onstatechange exists, call it
        jerry_value_t rval = jerry_call_function(func, obj, &new_state, 1);
        if (jerry_value_has_error_flag(rval)) {
            PRINT("zjs_sensor_set_state: error calling onstatechange\n");
        }
        jerry_release_value(rval);
    }
    jerry_release_value(func);

    if (old_state == SENSOR_STATE_ACTIVATING &&
        state == SENSOR_STATE_ACTIVATED) {
        func = zjs_get_property(obj, "onactivate");
        if (jerry_value_is_function(func)) {
            // if onactivate exists, call it
            jerry_value_t rval = jerry_call_function(func, obj, NULL, 0);
            if (jerry_value_has_error_flag(rval)) {
                PRINT("zjs_sensor_set_state: error calling onactivate\n");
            }
            jerry_release_value(rval);
        }
        jerry_release_value(func);
    }
    jerry_release_value(new_state);
}

static void zjs_sensor_update_reading(jerry_value_t obj,
                                      enum sensor_channel channel,
                                      double value[])
{
    // update reading property and trigger onchange event
    double x = value[0];
    double y = value[1];
    double z = value[2];
    jerry_value_t x_val = jerry_create_number(x);
    jerry_value_t y_val = jerry_create_number(y);
    jerry_value_t z_val = jerry_create_number(z);
    jerry_value_t reading_obj = jerry_create_object();
    if (channel == SENSOR_CHAN_ACCEL_ANY ||
        channel == SENSOR_CHAN_GYRO_ANY) {
        zjs_set_property(reading_obj, "x", x_val);
        zjs_set_property(reading_obj, "y", y_val);
        zjs_set_property(reading_obj, "z", z_val);
    }
    zjs_set_property(obj, "reading", reading_obj);
    jerry_value_t func = zjs_get_property(obj, "onchange");
    if (jerry_value_is_function(func)) {
        jerry_value_t event = jerry_create_object();
        // if onchange exists, call it
        zjs_set_property(event, "reading", reading_obj);
        jerry_value_t rval = jerry_call_function(func, obj, &event, 1);
        if (jerry_value_has_error_flag(rval)) {
            PRINT("zjs_sensor_update_reading: error calling onchange\n");
        }
        jerry_release_value(rval);
        jerry_release_value(event);
    }
    jerry_release_value(func);
    jerry_release_value(x_val);
    jerry_release_value(y_val);
    jerry_release_value(z_val);
    jerry_release_value(reading_obj);
}

static void zjs_sensor_trigger_error(jerry_value_t obj,
                                     const char *error_type)
{
    zjs_sensor_set_state(obj, SENSOR_STATE_ERRORED);
    jerry_value_t func = zjs_get_property(obj, "onerror");
    if (jerry_value_is_function(func)) {
        // if onerror exists, call it
        jerry_value_t event = jerry_create_string(error_type);
        jerry_value_t rval = jerry_call_function(func, obj, &event, 1);
        if (jerry_value_has_error_flag(rval)) {
            PRINT("zjs_sensor_trigger_error: error calling onerrorhange\n");
        }
        jerry_release_value(rval);
        jerry_release_value(event);
    }
    jerry_release_value(func);
}

static void zjs_sensor_onchange_c_callback(void *h)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    if (!handle) {
        PRINT("zjs_sensor_onchange_c_callback: handle not found\n");
        return;
    }
    zjs_sensor_update_reading(handle->sensor_obj,
                              handle->channel,
                              handle->sensor_value);
}

static void zjs_sensor_signal_callbacks(sensor_handle_t *handle, double value[])
{
    if (!handle)
        return;

    // iterate all sensor instances to update readings and trigger event
    for (sensor_handle_t *h = handle; h; h = h->next) {
        memcpy(h->sensor_value, value, sizeof(double) * 3);
        zjs_signal_callback(h->id);
    }
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_SENSOR)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t*)(*(uintptr_t *)data);
    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t*)msg->user_data;
        // synchrounus ipm, copy the results
        if (result) {
            memcpy(result, msg, sizeof(zjs_ipm_message_t));
        }
        // un-block sync api
        nano_isr_sem_give(&sensor_sem);
    } else if (msg->type == TYPE_SENSOR_EVENT_READING_CHANGE) {
        // value change event, copy the data, and signal event callback
        if (msg->data.sensor.channel == SENSOR_CHAN_ACCEL_ANY) {
            zjs_sensor_signal_callbacks(accel_handle, msg->data.sensor.value);
        }
        else if (msg->data.sensor.channel == SENSOR_CHAN_GYRO_ANY) {
            zjs_sensor_signal_callbacks(gyro_handle, msg->data.sensor.value);
        } else {
            PRINT("ipm_msg_receive_callback: unsupported sensor type\n");
        }
    } else {
        PRINT("ipm_msg_receive_callback: unsupported message received\n");
    }
}

static void zjs_sensor_callback_free(uintptr_t handle)
{
    zjs_free((sensor_handle_t *)handle);
}

static jerry_value_t zjs_sensor_start(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // requires: this is a Sensor object from takes no args
    //  effects: activates the sensor and start monitoring changes
    uintptr_t ptr;
    sensor_handle_t *handle = NULL;
    if (jerry_get_object_native_handle(this, &ptr)) {
         handle = (sensor_handle_t *)ptr;
         if (!handle) {
             return zjs_error("zjs_sensor_start: cannot find handle");
         }
    }

    enum sensor_state state = zjs_sensor_get_state(this);
    if (state != SENSOR_STATE_IDLE && state != SENSOR_STATE_ERRORED) {
        return zjs_error("zjs_sensor_start: invalid state");
    }

    zjs_sensor_set_state(this, SENSOR_STATE_ACTIVATING);
    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_START;
    send.data.sensor.channel = handle->channel;
    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        if (error == ERROR_IPM_OPERATION_DENIED) {
            zjs_sensor_trigger_error(this, "NotAllowedError");
        }
        else {
            // throw exception for all other errors
            return zjs_error("zjs_sensor_start: operation failed");
        }
    }
    zjs_sensor_set_state(this, SENSOR_STATE_ACTIVATED);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_sensor_stop(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // requires: this is a Sensor object from takes no args
    //  effects: de-activates the sensor and stop monitoring changes
    uintptr_t ptr;
    sensor_handle_t *handle = NULL;
    if (jerry_get_object_native_handle(this, &ptr)) {
         handle = (sensor_handle_t *)ptr;
         if (!handle) {
             return zjs_error("zjs_sensor_stop: cannot find handle");
         }
    }

    enum sensor_state state = zjs_sensor_get_state(this);
    if (state == SENSOR_STATE_IDLE || state == SENSOR_STATE_ERRORED) {
        return zjs_error("zjs_sensor_stop: invalid state");
    }

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_STOP;
    send.data.sensor.channel = handle->channel;
    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        // throw exception for all other errors
        return zjs_error("zjs_sensor_start: operation failed");
    }
    jerry_value_t reading_val = jerry_create_null();
    zjs_set_property(this, "reading", reading_val);
    jerry_release_value(reading_val);
    zjs_sensor_set_state(this, SENSOR_STATE_IDLE);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_sensor_create(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc,
                                       enum sensor_channel channel)
{
    double frequency = 50; // default frequency
    if (argc >= 1) {
        if (!jerry_value_is_object(argv[0]))
            return zjs_error("zjs_sensor_create: invalid argument");

        jerry_value_t options = argv[0];

        double option_freq;
        if (zjs_obj_get_double(options, "frequency", &option_freq)) {
            // TODO: figure out a list of frequencies we can support
            // and have Zephyr trigger value changes instead of
            // polling on the ARC.
            // For now, frequency is ignored,  we just report new event
            // as soon as we detect a value change.
            if (option_freq <= 0) {
                PRINT("zjs_sensor_create: invalid frequency, defaulting to 50hz\n");
            } else {
                frequency = option_freq;
            }
        } else {
            PRINT("zjs_sensor_create: frequency not found, defaulting to 50hz\n");
        }

        if (channel == SENSOR_CHAN_ACCEL_ANY) {
            bool option_gravity;
            if (zjs_obj_get_boolean(options, "includeGravity", &option_gravity) &&
                option_gravity) {
                // TODO: find out if BMI160 can be configured to include gravity
                PRINT("zjs_sensor_create: includeGravity is not supported\n");
            }
        }
    }

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_INIT;
    jerry_value_t result = zjs_sensor_call_remote_function(&send);
    if (jerry_value_has_error_flag(result)) {
        return result;
    }

    // initialize object and default values
    jerry_value_t sensor_obj = jerry_create_object();
    jerry_value_t frequency_val = jerry_create_number(frequency);
    jerry_value_t state_val = jerry_create_string("idle");
    jerry_value_t reading_val = jerry_create_null();
    zjs_set_property(sensor_obj, "frequency", frequency_val);
    zjs_set_property(sensor_obj, "state", state_val);
    zjs_set_property(sensor_obj, "reading", reading_val);
    jerry_release_value(frequency_val);
    jerry_release_value(state_val);
    jerry_release_value(reading_val);

    zjs_obj_add_function(sensor_obj, zjs_sensor_start, "start");
    zjs_obj_add_function(sensor_obj, zjs_sensor_stop, "stop");

    sensor_handle_t* handle = zjs_sensor_alloc_handle(channel);
    handle->id = zjs_add_c_callback(handle, zjs_sensor_onchange_c_callback);
    handle->channel = channel;
    handle->sensor_obj = sensor_obj;

    // watch for the object getting garbage collected, and clean up
    jerry_set_object_native_handle(sensor_obj, (uintptr_t)handle,
                                   zjs_sensor_callback_free);

    return sensor_obj;
}

static jerry_value_t zjs_accel_create(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // requires: arg 0 is an object containing sensor options:
    //             frequency (double) - sampling frequency, default to 50
    //             includeGravity (bool) - whether you want gravity included
    //  effects: Creates a Accelerometer object to the local sensor
    return zjs_sensor_create(function_obj,
                             this,
                             argv,
                             argc,
                             SENSOR_CHAN_ACCEL_ANY);
}

static jerry_value_t zjs_gyro_create(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // requires: arg 0 is an object containing sensor options:
    //             frequency (double) - sampling frequency, default to 50
    //  effects: Creates a Gyroscope object to the local sensor
    return zjs_sensor_create(function_obj,
                             this,
                             argv,
                             argc,
                             SENSOR_CHAN_GYRO_ANY);
}

void zjs_sensor_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_SENSOR, ipm_msg_receive_callback);

    nano_sem_init(&sensor_sem);

    jerry_value_t global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_accel_create, "Accelerometer");
    zjs_obj_add_function(global_obj, zjs_gyro_create, "Gyroscope");
    jerry_release_value(global_obj);
}

void zjs_sensor_cleanup()
{
    if (accel_handle) {
        zjs_sensor_free_handles(accel_handle);
    }
    if (gyro_handle) {
        zjs_sensor_free_handles(gyro_handle);
    }
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_SENSOR
