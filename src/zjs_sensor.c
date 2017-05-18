// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_SENSOR
#ifndef QEMU_BUILD
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#include <string.h>

// ZJS includes
#include "zjs_common.h"
#include "zjs_callbacks.h"
#include "zjs_ipm.h"
#include "zjs_util.h"
#include "zjs_sensor.h"
#ifdef BUILD_MODULE_SENSOR_ACCEL
#include "zjs_sensor_accel.h"
#endif
#ifdef BUILD_MODULE_SENSOR_GYRO
#include "zjs_sensor_gyro.h"
#endif
#ifdef BUILD_MODULE_SENSOR_LIGHT
#include "zjs_sensor_light.h"
#endif
#ifdef BUILD_MODULE_SENSOR_TEMP
#include "zjs_sensor_temp.h"
#endif

#define ZJS_SENSOR_TIMEOUT_TICKS 5000

static struct k_sem sensor_sem;

static jerry_value_t zjs_sensor_prototype;

typedef sensor_instance_t* (*initcb_t)();
typedef void (*cleanupcb_t)();

typedef struct sensor_module {
    enum sensor_channel channel;
    initcb_t init;
    cleanupcb_t cleanup;
    sensor_instance_t *instance;
} sensor_module_t;

sensor_module_t sensor_modules[] = {
#ifdef BUILD_MODULE_SENSOR_ACCEL
    { SENSOR_CHAN_ACCEL_XYZ, zjs_sensor_accel_init, zjs_sensor_accel_cleanup },
#endif
#ifdef BUILD_MODULE_SENSOR_GYRO
    { SENSOR_CHAN_GYRO_XYZ, zjs_sensor_gyro_init, zjs_sensor_gyro_cleanup },
#endif
#ifdef BUILD_MODULE_SENSOR_LIGHT
    { SENSOR_CHAN_LIGHT, zjs_sensor_light_init, zjs_sensor_light_cleanup },
#endif
#ifdef BUILD_MODULE_SENSOR_TEMP
    { SENSOR_CHAN_TEMP, zjs_sensor_temp_init, zjs_sensor_temp_cleanup },
#endif
};

static const jerry_object_native_info_t sensor_type_info =
{
   .free_cb = free_handle_nop
};

sensor_instance_t *zjs_sensor_create_instance(const char *name, void *func)
{
    ZVAL global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, func, name);
    sensor_instance_t *instance = zjs_malloc(sizeof(sensor_instance_t));
    if (!instance) {
        ERR_PRINT("failed to allocate sensor_instance_t\n");
        return NULL;
    }
    memset(instance, 0, sizeof(sensor_instance_t));
    return instance;
}

static sensor_handle_t *zjs_sensor_alloc_handle()
{
    size_t size = sizeof(sensor_handle_t);
    sensor_handle_t *handle = zjs_malloc(size);
    if (!handle) {
        ERR_PRINT("cannot allocate sensor_handle_t\n");
        return NULL;
    }
    memset(handle, 0, size);
    return handle;
}

static void zjs_sensor_free_handles(sensor_handle_t *handles)
{
    sensor_handle_t *tmp;
    while (handles) {
        tmp = handles;
        if (tmp->state == SENSOR_STATE_ACTIVATED) {
            // we need to stop all sensors so that ARC
            // do not continue to send change events
            zjs_sensor_stop_sensor(tmp->sensor_obj);
        }
        if (tmp->controller) {
            zjs_free(tmp->controller);
        }
        zjs_remove_callback(tmp->onchange_cb_id);
        zjs_remove_callback(tmp->onstart_cb_id);
        zjs_remove_callback(tmp->onstop_cb_id);
        jerry_release_value(tmp->sensor_obj);
        handles = handles->next;
        zjs_free(tmp);
    }
}

static bool zjs_sensor_ipm_send_sync(zjs_ipm_message_t *send,
                                     zjs_ipm_message_t *result)
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

static int zjs_sensor_call_remote_function(zjs_ipm_message_t *send)
{
    zjs_ipm_message_t reply;
    if (!zjs_sensor_ipm_send_sync(send, &reply)) {
        return ERROR_IPM_OPERATION_FAILED;
    }
    return reply.error_code;
}

sensor_state_t zjs_sensor_get_state(jerry_value_t obj)
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

void zjs_sensor_set_state(jerry_value_t obj, sensor_state_t state)
{
    sensor_state_t old_state = zjs_sensor_get_state(obj);
    if (old_state == state) {
        return;
    }

    // update state property and trigger onactivate event if necessary
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

    if (old_state == SENSOR_STATE_ACTIVATING &&
        state == SENSOR_STATE_ACTIVATED) {
        ZVAL activate_func = zjs_get_property(obj, "onactivate");
        if (jerry_value_is_function(activate_func)) {
            // if onactivate exists, call it
            zjs_callback_id id = zjs_add_callback_once(activate_func, obj,
                                                       NULL, NULL);
            zjs_signal_callback(id, NULL, 0);
        }
    }

    sensor_handle_t *handle = NULL;
    const jerry_object_native_info_t *tmp;
    if (!jerry_get_object_native_pointer(obj, (void **)&handle, &tmp)) {
        ERR_PRINT("no handle found\n");
        return;
    }
    if (tmp != &sensor_type_info) {
        ERR_PRINT("handle type did not match\n");
        return;
    }
    handle->state = state;
}

void zjs_sensor_trigger_change(jerry_value_t obj)
{
    uint64_t timestamp = k_uptime_get();
    zjs_obj_add_readonly_number(obj, ((double)timestamp), "timestamp");

    ZVAL func = zjs_get_property(obj, "onchange");
    if (jerry_value_is_function(func)) {
        ZVAL event = jerry_create_object();
        // if onchange exists, call it
        ZVAL rval = jerry_call_function(func, obj, NULL, 0);
        if (jerry_value_has_error_flag(rval)) {
            ERR_PRINT("calling onchange\n");
        }
    }
}

void zjs_sensor_trigger_error(jerry_value_t obj,
                              const char *error_name,
                              const char *error_message)
{
    zjs_sensor_set_state(obj, SENSOR_STATE_ERRORED);
    ZVAL func = zjs_get_property(obj, "onerror");
    if (jerry_value_is_function(func)) {
        // if onerror exists, call it
        ZVAL event = jerry_create_object();
        ZVAL error_obj = jerry_create_object();
        ZVAL name_val = jerry_create_string(error_name);
        ZVAL message_val = jerry_create_string(error_message);
        zjs_set_property(error_obj, "name", name_val);
        zjs_set_property(error_obj, "message", message_val);
        zjs_set_property(event, "error", error_obj);
        zjs_callback_id id = zjs_add_callback_once(func, obj, NULL, NULL);
        zjs_signal_callback(id, &event, sizeof(event));
        DBG_PRINT("triggering error %s (%s)\n", error_name, error_message);
    }
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void zjs_sensor_signal_callbacks(struct sensor_data *data)
{
    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    for (int i = 0; i < modcount; i++) {
          sensor_module_t *mod = &sensor_modules[i];
          if (mod->channel == data->channel) {
              sensor_handle_t *handles = mod->instance->handles;
              // iterate all sensor handles to update readings and trigger event
              for (sensor_handle_t *h = handles; h; h = h->next) {
                  if (h->state == SENSOR_STATE_ACTIVATED) {
                      zjs_signal_callback(h->onchange_cb_id,
                                          &data->reading,
                                          sizeof(data->reading));
                  }
              }
              return;
          }
    }

    ERR_PRINT("unsupported sensor type\n");
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_SENSOR)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);
    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;
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

jerry_value_t zjs_sensor_start_sensor(jerry_value_t obj)
{
    ZJS_GET_HANDLE(obj, sensor_handle_t, handle, sensor_type_info);

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_START;
    send.data.sensor.channel = handle->channel;
    send.data.sensor.frequency = handle->frequency;
    if (handle->controller) {
        send.data.sensor.controller = handle->controller->name;
        send.data.sensor.pin = handle->controller->pin;
    }

    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        if (error == ERROR_IPM_OPERATION_NOT_ALLOWED) {
            return zjs_custom_error("NotAllowedError", "permission denied");
        } else {
            return zjs_custom_error("UnknownError", "IPM failed");
        }
    }

    zjs_sensor_set_state(handle->sensor_obj, SENSOR_STATE_ACTIVATED);
    DBG_PRINT("sensor driver %s started\n", handle->controller->name);
    return ZJS_UNDEFINED;
}

jerry_value_t zjs_sensor_stop_sensor(jerry_value_t obj)
{
    ZJS_GET_HANDLE(obj, sensor_handle_t, handle, sensor_type_info);

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_STOP;
    send.data.sensor.channel = handle->channel;
    if (handle->controller) {
        send.data.sensor.controller = handle->controller->name;
        send.data.sensor.pin = handle->controller->pin;
    }

    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        if (error == ERROR_IPM_OPERATION_NOT_ALLOWED) {
            return zjs_custom_error("NotAllowedError", "permission denied");
        } else {
            return zjs_custom_error("UnknownError", "IPM failed");
        }
    }

    zjs_sensor_set_state(handle->sensor_obj, SENSOR_STATE_IDLE);
    DBG_PRINT("sensor driver %s stopped\n", handle->controller->name);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_sensor_start)
{
    // requires: this is a Sensor object from takes no args
    //  effects: activates the sensor and start monitoring changes
    sensor_state_t state = zjs_sensor_get_state(this);
    if (state == SENSOR_STATE_ACTIVATING || state == SENSOR_STATE_ACTIVATED) {
        return ZJS_UNDEFINED;
    }

    ZJS_GET_HANDLE(this, sensor_handle_t, handle, sensor_type_info);

    zjs_sensor_set_state(this, SENSOR_STATE_ACTIVATING);
    if (jerry_value_has_error_flag(zjs_sensor_start_sensor(this))) {
        zjs_sensor_trigger_error(this, "SensorError", "start failed");
    }

    zjs_signal_callback(handle->onstart_cb_id, NULL, 0);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_sensor_stop)
{
    // requires: this is a Sensor object from takes no args
    //  effects: de-activates the sensor and stop monitoring changes
    sensor_state_t state = zjs_sensor_get_state(this);
    if (state != SENSOR_STATE_ACTIVATING && state != SENSOR_STATE_ACTIVATED) {
        return ZJS_UNDEFINED;
    }

    ZJS_GET_HANDLE(this, sensor_handle_t, handle, sensor_type_info);

    if (jerry_value_has_error_flag(zjs_sensor_start_sensor(this))) {
        zjs_sensor_trigger_error(this, "SensorError", "start failed");
    }

    zjs_sensor_set_state(this, SENSOR_STATE_IDLE);
    zjs_signal_callback(handle->onstop_cb_id, NULL, 0);
    return ZJS_UNDEFINED;
}


ZJS_DECL_FUNC_ARGS(zjs_sensor_create,
                   sensor_instance_t *instance,
                   enum sensor_channel channel,
                   const char *c_name,
                   uint32_t pin,
                   uint32_t max_frequency,
                   zjs_c_callback_func onchange,
                   zjs_c_callback_func onstart,
                   zjs_c_callback_func onstop)
{
    if (!instance) {
        return zjs_error("sensor instance not found");
    }

    double frequency = DEFAULT_SAMPLING_FREQUENCY;
    size_t size = SENSOR_MAX_CONTROLLER_NAME_LEN;
    sensor_controller_t controller;

    if (argc >= 1) {
        jerry_value_t options = argv[0];

        ZVAL controller_val = zjs_get_property(options, "controller");
        if (jerry_value_is_string(controller_val)) {
            zjs_copy_jstring(controller_val, controller.name, &size);
        } else if (c_name) {
            snprintf(controller.name, size, "%s", c_name);
            DBG_PRINT("controller not set, default to %s\n", controller.name);
        } else {
            return zjs_error("controller not found");
        }


        uint32_t option_pin;
        if (zjs_obj_get_uint32(options, "pin", &option_pin)) {
            controller.pin = option_pin;
        } else if (pin != -1) {
            controller.pin = pin;
        } else {
            return zjs_error("pin not found");
        }

        double option_freq;
        if (zjs_obj_get_double(options, "frequency", &option_freq)) {
            if (option_freq < 1 || option_freq > max_frequency) {
                ERR_PRINT("unsupported frequency, default to %dHz\n",
                          DEFAULT_SAMPLING_FREQUENCY);
            } else {
                frequency = option_freq;
            }
        } else {
            DBG_PRINT("frequency not found, default to %dHz\n",
                      DEFAULT_SAMPLING_FREQUENCY);
        }
    }

    zjs_ipm_message_t send;
    send.type = TYPE_SENSOR_INIT;
    send.data.sensor.channel = channel;
    send.data.sensor.controller = controller.name;
    send.data.sensor.pin = controller.pin;

    int error = zjs_sensor_call_remote_function(&send);
    if (error != ERROR_IPM_NONE) {
        return zjs_error("zjs_sensor_create failed to init\n");
    }

    // initialize object and default values
    jerry_value_t sensor_obj = jerry_create_object();
    ZVAL null_val = jerry_create_null();

    zjs_set_readonly_property(sensor_obj, "timestamp", null_val);
    zjs_obj_add_number(sensor_obj, frequency, "frequency");
    zjs_obj_add_readonly_string(sensor_obj, "unconnected", "state");
    jerry_set_prototype(sensor_obj, zjs_sensor_prototype);

    sensor_handle_t *handle = zjs_sensor_alloc_handle(&(instance->handles));
    handle->controller = zjs_malloc(sizeof(sensor_controller_t));
    memcpy(handle->controller, &controller, sizeof(sensor_controller_t));
    handle->channel = channel;
    handle->frequency = frequency;
    handle->sensor_obj = jerry_acquire_value(sensor_obj);
    handle->onchange_cb_id = zjs_add_c_callback(handle, onchange);
    handle->onstart_cb_id = zjs_add_c_callback(handle, onstart);
    handle->onstop_cb_id = zjs_add_c_callback(handle, onstop);

    // append to the list kept in the instance
    handle->next = instance->handles;
    instance->handles = handle;

    // watch for the object getting garbage collected, and clean up
    jerry_set_object_native_pointer(sensor_obj, handle, &sensor_type_info);

    DBG_PRINT("sensor driver %s initialized\n", handle->controller->name);
    DBG_PRINT("sensor frequency %u\n", handle->frequency);
    return sensor_obj;
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

    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    for (int i = 0; i < modcount; i++) {
        sensor_module_t *mod = &sensor_modules[i];
        if (!mod->instance) {
            mod->instance = mod->init();
        }
    }
}

void zjs_sensor_cleanup()
{
    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    for (int i = 0; i < modcount; i++) {
        sensor_module_t *mod = &sensor_modules[i];
        if (mod->instance) {
            zjs_sensor_free_handles(mod->instance->handles);
        }
        mod->cleanup();
        mod->instance = NULL;
    }
    jerry_release_value(zjs_sensor_prototype);
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_SENSOR
