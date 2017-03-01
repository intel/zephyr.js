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

#define ZJS_SENSOR_TIMEOUT_TICKS 5000
#define DEFAULT_SAMPLING_FREQUENCY 20

static struct k_sem sensor_sem;

static jerry_value_t zjs_sensor_prototype;

enum sensor_state {
    SENSOR_STATE_UNCONNECTED,
    SENSOR_STATE_IDLE,
    SENSOR_STATE_ACTIVATING,
    SENSOR_STATE_ACTIVATED,
    SENSOR_STATE_ERRORED,
};

typedef struct sensor_handle {
    zjs_callback_id onchange_cb_id;
    zjs_callback_id onstart_cb_id;
    zjs_callback_id onstop_cb_id;
    enum sensor_channel channel;
    int frequency;
    enum sensor_state state;
    jerry_value_t sensor_obj;
    struct sensor_handle *next;
} sensor_handle_t;

static sensor_handle_t *accel_handles = NULL;
static sensor_handle_t *gyro_handles = NULL;
static sensor_handle_t *light_handles = NULL;

static sensor_handle_t *zjs_sensor_alloc_handle(enum sensor_channel channel)
{
    size_t size = sizeof(sensor_handle_t);
    sensor_handle_t *handle = zjs_malloc(size);
    if (handle) {
        memset(handle, 0, size);
    }

    // append to the list
    sensor_handle_t **head = NULL;
    if (channel == SENSOR_CHAN_ACCEL_XYZ) {
        head = &accel_handles;
    } else if (channel == SENSOR_CHAN_GYRO_XYZ) {
        head = &gyro_handles;
    } else if (channel == SENSOR_CHAN_LIGHT) {
        head = &light_handles;
    } else {
        ERR_PRINT("invalid channel\n");
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

static sensor_handle_t *zjs_sensor_get_handle(jerry_value_t obj)
{
    uintptr_t ptr;
    sensor_handle_t *handle = NULL;
    if (jerry_get_object_native_handle(obj, &ptr)) {
        handle = (sensor_handle_t *)ptr;
        if (handle) {
            return handle;
        }
    }
    ERR_PRINT("cannot find handle");
    return NULL;
}

static bool zjs_sensor_ipm_send_sync(zjs_ipm_message_t* send,
                                     zjs_ipm_message_t* result)
{
    send->id = MSG_ID_SENSOR;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_SENSOR, send) != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (k_sem_take(&sensor_sem, ZJS_SENSOR_TIMEOUT_TICKS)) {
        ERR_PRINT("FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static int zjs_sensor_call_remote_function(zjs_ipm_message_t* send)
{
    zjs_ipm_message_t reply;
    if (!zjs_sensor_ipm_send_sync(send, &reply)) {
        return ERROR_IPM_OPERATION_FAILED;
    }
    return reply.error_code;
}

static enum sensor_state zjs_sensor_get_state(jerry_value_t obj)
{
    const int BUFLEN = 20;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "state", buffer, BUFLEN)) {
        if (!strcmp(buffer, "unconnected"))
            return SENSOR_STATE_UNCONNECTED;
        if (!strcmp(buffer, "idle"))
            return SENSOR_STATE_IDLE;
        if (!strcmp(buffer, "activating"))
            return SENSOR_STATE_ACTIVATING;
        if (!strcmp(buffer, "activated"))
            return SENSOR_STATE_ACTIVATED;
        if (!strcmp(buffer, "errored"))
            return SENSOR_STATE_ERRORED;
        else
            ERR_PRINT("invalid state set %s\n", buffer);
    } else {
        ERR_PRINT("state is undefined\n");
    }

    zjs_obj_add_readonly_string(obj, "errored", "state");
    return SENSOR_STATE_ERRORED;
}

static void zjs_sensor_set_state(jerry_value_t obj, enum sensor_state state)
{
    enum sensor_state old_state = zjs_sensor_get_state(obj);
    if (old_state == state) {
        return;
    }

    // update state property and trigger onstatechange event
    const char *state_str = NULL;
    switch(state) {
    case SENSOR_STATE_UNCONNECTED:
        state_str = "unconnected";
        break;
    case SENSOR_STATE_IDLE:
        state_str = "idle";
        break;
    case SENSOR_STATE_ACTIVATING:
        state_str = "activating";
        break;
    case SENSOR_STATE_ACTIVATED:
        state_str = "activated";
        break;
    case SENSOR_STATE_ERRORED:
        state_str = "errored";
        break;

    default:
        // should never get here
        ERR_PRINT("invalid state\n");
        return;
    }
    zjs_obj_add_readonly_string(obj, state_str, "state");

    jerry_value_t func = zjs_get_property(obj, "onstatechange");
    if (jerry_value_is_function(func)) {
        // if onstatechange exists, call it
        jerry_value_t new_state = jerry_create_string(state_str);
        zjs_callback_id id = zjs_add_callback_once(func, obj, NULL, NULL);
        zjs_signal_callback(id, &new_state, sizeof(new_state));
        jerry_release_value(new_state);
    }
    jerry_release_value(func);

    if (old_state == SENSOR_STATE_ACTIVATING &&
        state == SENSOR_STATE_ACTIVATED) {
        func = zjs_get_property(obj, "onactivate");
        if (jerry_value_is_function(func)) {
            // if onactivate exists, call it
            zjs_callback_id id = zjs_add_callback_once(func, obj, NULL, NULL);
            zjs_signal_callback(id, NULL, 0);
        }
        jerry_release_value(func);
    }
}

static void zjs_sensor_update_reading(jerry_value_t obj,
                                      enum sensor_channel channel,
                                      void *reading)
{
    // update reading property and trigger onchange event
    jerry_value_t reading_obj = jerry_create_object();
    switch(channel) {
    case SENSOR_CHAN_ACCEL_XYZ:
    case SENSOR_CHAN_GYRO_XYZ: ;
        // reading is a ptr to an array of 3 double values
        double x = ((double*)reading)[0];
        double y = ((double*)reading)[1];
        double z = ((double*)reading)[2];
        zjs_obj_add_readonly_number(reading_obj, x, "x");
        zjs_obj_add_readonly_number(reading_obj, y, "y");
        zjs_obj_add_readonly_number(reading_obj, z, "z");
        break;
    case SENSOR_CHAN_LIGHT: ;
        // reading is a ptr to double
        double d = *((double*)reading);
        zjs_obj_add_readonly_number(reading_obj, d, "illuminance");
        break;

    default:
        ERR_PRINT("unsupported sensor type\n");
        jerry_release_value(reading_obj);
        return;
    }

    zjs_set_property(obj, "reading", reading_obj);
    jerry_value_t func = zjs_get_property(obj, "onchange");
    if (jerry_value_is_function(func)) {
        jerry_value_t event = jerry_create_object();
        // if onchange exists, call it
        zjs_set_property(event, "reading", reading_obj);
        jerry_value_t rval = jerry_call_function(func, obj, &event, 1);
        if (jerry_value_has_error_flag(rval)) {
            ERR_PRINT("calling onchange\n");
        }
        jerry_release_value(rval);
        jerry_release_value(event);
    }
    jerry_release_value(func);
    jerry_release_value(reading_obj);
}

static void zjs_sensor_trigger_error(jerry_value_t obj,
                                     const char *error_name,
                                     const char *error_message)
{
    zjs_sensor_set_state(obj, SENSOR_STATE_ERRORED);
    jerry_value_t func = zjs_get_property(obj, "onerror");
    if (jerry_value_is_function(func)) {
        // if onerror exists, call it
        jerry_value_t event = jerry_create_object();
        jerry_value_t error_obj = jerry_create_object();
        jerry_value_t name_val = jerry_create_string(error_name);
        jerry_value_t message_val = jerry_create_string(error_message);
        zjs_set_property(error_obj, "name", name_val);
        zjs_set_property(error_obj, "message", message_val);
        zjs_set_property(event, "error", error_obj);
        zjs_callback_id id = zjs_add_callback_once(func, obj, NULL, NULL);
        zjs_signal_callback(id, &event, 1);
        jerry_release_value(name_val);
        jerry_release_value(message_val);
        jerry_release_value(error_obj);
        jerry_release_value(event);
    }
    jerry_release_value(func);
}

static void zjs_sensor_onchange_c_callback(void *h, void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    if (!handle) {
        ERR_PRINT("handle not found\n");
        return;
    }

    if (zjs_sensor_get_state(handle->sensor_obj) == SENSOR_STATE_ACTIVATED) {
        zjs_sensor_update_reading(handle->sensor_obj,
                                  handle->channel,
                                  argv);
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void zjs_sensor_signal_callbacks(struct sensor_data *data)
{
    sensor_handle_t *handles = NULL;
    bool xyz = false;

    switch(data->channel) {
    case SENSOR_CHAN_ACCEL_XYZ:
        handles = accel_handles;
        xyz = true;
        break;
    case SENSOR_CHAN_GYRO_XYZ:
        handles = gyro_handles;
        xyz = true;
        break;
    case SENSOR_CHAN_LIGHT:
        handles = light_handles;
        break;

    default:
        ERR_PRINT("unsupported sensor type\n");
        return;
    }

    // iterate all sensor instances to update readings and trigger event
    for (sensor_handle_t *h = handles; h; h = h->next) {
        if (xyz) {
            double xyz[3];
            xyz[0] = data->reading.x;
            xyz[1] = data->reading.y;
            xyz[2] = data->reading.z;
            zjs_signal_callback(h->onchange_cb_id, &xyz, sizeof(xyz));
        } else {
            double dval = data->reading.dval;
            zjs_signal_callback(h->onchange_cb_id, &dval, sizeof(dval));
        }
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
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
        k_sem_give(&sensor_sem);
    } else if (msg->type == TYPE_SENSOR_EVENT_READING_CHANGE) {
        // value change event, signal event callback
        zjs_sensor_signal_callbacks(&msg->data.sensor);
    } else {
        ERR_PRINT("unsupported message received\n");
    }
}

static void zjs_sensor_callback_free(uintptr_t handle)
{
    zjs_free((sensor_handle_t *)handle);
}

static void zjs_sensor_onstart_c_callback(void *h, void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    if (!handle) {
        ERR_PRINT("handle not found\n");
        return;
    }

    jerry_value_t obj = handle->sensor_obj;
    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_START;
    send.data.sensor.channel = handle->channel;
    send.data.sensor.frequency = handle->frequency;
    if (handle->channel == SENSOR_CHAN_LIGHT) {
        // AmbientLightSensor needs provide AIO pin value
        uint32_t pin;
        if (!zjs_obj_get_uint32(obj, "pin", &pin)) {
            zjs_sensor_trigger_error(obj, "DataError",
                                     "pin not found");
            return;
        }
        send.data.sensor.pin = pin;
    }

    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        if (error == ERROR_IPM_OPERATION_NOT_ALLOWED) {
            zjs_sensor_trigger_error(obj, "NotAllowedError",
                                     "permission denied");
        } else {
            zjs_sensor_trigger_error(obj, "UnknownError",
                                     "IPM failed");
        }
    }

    zjs_sensor_set_state(obj, SENSOR_STATE_ACTIVATED);
}

static void zjs_sensor_onstop_c_callback(void *h, void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    if (!handle) {
        ERR_PRINT("handle not found\n");
        return;
    }

    jerry_value_t obj = handle->sensor_obj;
    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_STOP;
    send.data.sensor.channel = handle->channel;

    jerry_value_t reading_val = jerry_create_null();
    zjs_set_property(obj, "reading", reading_val);
    jerry_release_value(reading_val);
    if (handle->channel == SENSOR_CHAN_LIGHT) {
        // AmbientLightSensor needs provide AIO pin value
        uint32_t pin;
        if (!zjs_obj_get_uint32(obj, "pin", &pin)) {
            zjs_sensor_trigger_error(obj, "DataError",
                                     "pin not found");
            return;
        }
        send.data.sensor.pin = pin;
    }

    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        if (error == ERROR_IPM_OPERATION_NOT_ALLOWED) {
            zjs_sensor_trigger_error(obj, "NotAllowedError",
                                     "permission denied");
        } else {
            zjs_sensor_trigger_error(obj, "UnknownError",
                                     "IPM failed");
        }
        zjs_sensor_set_state(obj, SENSOR_STATE_ERRORED);
    }
}

static jerry_value_t zjs_sensor_start(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // requires: this is a Sensor object from takes no args
    //  effects: activates the sensor and start monitoring changes
    enum sensor_state state = zjs_sensor_get_state(this);
    if (state == SENSOR_STATE_ACTIVATING || state == SENSOR_STATE_ACTIVATED) {
        return ZJS_UNDEFINED;
    }

    sensor_handle_t *handle = zjs_sensor_get_handle(this);
    if (!handle) {
         return zjs_error("cannot find handle");
    }

    zjs_sensor_set_state(this, SENSOR_STATE_ACTIVATING);
    zjs_signal_callback(handle->onstart_cb_id, NULL, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_sensor_stop(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // requires: this is a Sensor object from takes no args
    //  effects: de-activates the sensor and stop monitoring changes
    enum sensor_state state = zjs_sensor_get_state(this);
    if (state != SENSOR_STATE_ACTIVATING && state != SENSOR_STATE_ACTIVATED) {
        return ZJS_UNDEFINED;
    }

    sensor_handle_t *handle = zjs_sensor_get_handle(this);
    if (!handle) {
         return zjs_error("cannot find handle");
    }

    // we have to set state to idle first and then check if error occued
    // in the onstop_c_callback()
    zjs_sensor_set_state(this, SENSOR_STATE_IDLE);
    zjs_signal_callback(handle->onstop_cb_id, NULL, 0);
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_sensor_create(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc,
                                       enum sensor_channel channel)
{
    double frequency = DEFAULT_SAMPLING_FREQUENCY;
    uint32_t pin = -1;

    if (argc < 1 && channel == SENSOR_CHAN_LIGHT) {
        // AmbientLightSensor requires ADC pin object
        return zjs_error("zjs_sensor_create: invalid argument\n");
    }

    if (argc >= 1) {
        if (!jerry_value_is_object(argv[0]))
            return zjs_error("zjs_sensor_create: invalid argument");

        jerry_value_t options = argv[0];

        double option_freq;
        if (zjs_obj_get_double(options, "frequency", &option_freq)) {
            if (option_freq < 1) {
                ERR_PRINT("unsupported frequency, default to %dHz\n",
                          DEFAULT_SAMPLING_FREQUENCY);
            } else {
                frequency = option_freq;
            }
        } else {
            DBG_PRINT("frequency not found, default to %dHz\n",
                      DEFAULT_SAMPLING_FREQUENCY);
        }

        if (channel == SENSOR_CHAN_ACCEL_XYZ) {
            bool option_gravity;
            if (zjs_obj_get_boolean(options, "includeGravity", &option_gravity) &&
                option_gravity) {
                // TODO: find out if BMI160 can be configured to include gravity
                ERR_PRINT("includeGravity is not supported\n");
            }
        }

        if (channel == SENSOR_CHAN_LIGHT) {
            if (!zjs_obj_get_uint32(options, "pin", &pin)) {
                pin = -1;
            }
        }
    }

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_INIT;
    send.data.sensor.channel = channel;
    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        return zjs_error("zjs_sensor_create failed to init\n");
    }

    // initialize object and default values
    jerry_value_t sensor_obj = jerry_create_object();
    jerry_value_t reading_val = jerry_create_null();
    zjs_obj_add_number(sensor_obj, frequency, "frequency");
    zjs_obj_add_readonly_string(sensor_obj, "unconnected", "state");
    zjs_set_property(sensor_obj, "reading", reading_val);
    jerry_release_value(reading_val);

    if (channel == SENSOR_CHAN_LIGHT && pin != -1) {
        zjs_obj_add_number(sensor_obj, pin, "pin");
    }

    jerry_set_prototype(sensor_obj, zjs_sensor_prototype);

    sensor_handle_t* handle = zjs_sensor_alloc_handle(channel);
    handle->onchange_cb_id = zjs_add_c_callback(handle, zjs_sensor_onchange_c_callback);
    handle->onstart_cb_id = zjs_add_c_callback(handle, zjs_sensor_onstart_c_callback);
    handle->onstop_cb_id = zjs_add_c_callback(handle, zjs_sensor_onstop_c_callback);
    handle->channel = channel;
    handle->frequency = frequency;
    handle->sensor_obj = jerry_acquire_value(sensor_obj);

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
    //             frequency (double) - sampling frequency
    //             includeGravity (bool) - whether you want gravity included
    //  effects: Creates a Accelerometer object to the local sensor
    return zjs_sensor_create(function_obj,
                             this,
                             argv,
                             argc,
                             SENSOR_CHAN_ACCEL_XYZ);
}

static jerry_value_t zjs_gyro_create(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc)
{
    // requires: arg 0 is an object containing sensor options:
    //             frequency (double) - sampling frequency
    //  effects: Creates a Gyroscope object to the local sensor
    return zjs_sensor_create(function_obj,
                             this,
                             argv,
                             argc,
                             SENSOR_CHAN_GYRO_XYZ);
}

static jerry_value_t zjs_light_create(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc)
{
    // requires: arg 0 is an object containing sensor options:
    //             frequency (double) - sampling frequency
    //  effects: Creates a AmbientLightSensor object to the local sensor
    return zjs_sensor_create(function_obj,
                             this,
                             argv,
                             argc,
                             SENSOR_CHAN_LIGHT);
}

void zjs_sensor_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_SENSOR, ipm_msg_receive_callback);

    k_sem_init(&sensor_sem, 0, 1);

    zjs_native_func_t array[] = {
        { zjs_sensor_start, "start" },
        { zjs_sensor_stop, "stop" },
        { NULL, NULL }
    };
    zjs_sensor_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_sensor_prototype, array);

    // create global objects
    jerry_value_t global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_accel_create, "Accelerometer");
    zjs_obj_add_function(global_obj, zjs_gyro_create, "Gyroscope");
    zjs_obj_add_function(global_obj, zjs_light_create, "AmbientLightSensor");
    jerry_release_value(global_obj);
}

void zjs_sensor_cleanup()
{
    if (accel_handles) {
        zjs_sensor_free_handles(accel_handles);
    }
    if (gyro_handles) {
        zjs_sensor_free_handles(gyro_handles);
    }
    if (light_handles) {
        zjs_sensor_free_handles(light_handles);
    }

    jerry_release_value(zjs_sensor_prototype);
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_SENSOR
