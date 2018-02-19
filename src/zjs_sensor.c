// Copyright (c) 2016-2018, Intel Corporation.

// C includes
#include <string.h>

#ifndef QEMU_BUILD
#ifndef ZJS_LINUX_BUILD

// Zephyr includes
#include <zephyr.h>
#endif

// ZJS includes
#include "zjs_board.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_sensor.h"
#include "zjs_sensor_board.h"
#include "zjs_util.h"

static jerry_value_t zjs_sensor_prototype;

static void zjs_sensor_free_cb(void *native)
{
    sensor_handle_t *handle = (sensor_handle_t *)native;
    if (handle->controller) {
        zjs_free(handle->controller);
    }
    zjs_remove_callback(handle->onchange_cb_id);
    zjs_remove_callback(handle->onstart_cb_id);
    zjs_remove_callback(handle->onstop_cb_id);
    zjs_free(handle);
}

static const jerry_object_native_info_t sensor_type_info = {
    .free_cb = zjs_sensor_free_cb
};

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
            if (zjs_sensor_board_stop(handles) != 0) {
                ERR_PRINT("cannot stop sensor\n");
            }
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

sensor_instance_t *zjs_sensor_create_instance(const char *name, void *func)
{
    ZVAL global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, name, func);
    sensor_instance_t *instance = zjs_malloc(sizeof(sensor_instance_t));
    if (!instance) {
        ERR_PRINT("failed to allocate sensor_instance_t\n");
        return NULL;
    }
    memset(instance, 0, sizeof(sensor_instance_t));
    return instance;
}

void zjs_sensor_free_instance(sensor_instance_t *instance)
{
    if (!instance)
        return;

    zjs_sensor_free_handles(instance->handles);
    zjs_free(instance);
}

sensor_state_t zjs_sensor_get_state(jerry_value_t obj)
{
    const int BUFLEN = 20;
    char buffer[BUFLEN];
    if (zjs_obj_get_string(obj, "state", buffer, BUFLEN)) {
        if (strequal(buffer, "unconnected"))
            return SENSOR_STATE_UNCONNECTED;
        if (strequal(buffer, "idle"))
            return SENSOR_STATE_IDLE;
        if (strequal(buffer, "activating"))
            return SENSOR_STATE_ACTIVATING;
        if (strequal(buffer, "activated"))
            return SENSOR_STATE_ACTIVATED;
        if (strequal(buffer, "errored"))
            return SENSOR_STATE_ERRORED;
        else
            ERR_PRINT("invalid state set %s\n", buffer);
    } else {
        ERR_PRINT("state is undefined\n");
    }

    zjs_obj_add_readonly_string(obj, "state", "errored");
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
    switch (state) {
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
    zjs_obj_add_readonly_string(obj, "state", state_str);

    if (old_state == SENSOR_STATE_ACTIVATING &&
        state == SENSOR_STATE_ACTIVATED) {
        ZVAL activate_func = zjs_get_property(obj, "onactivate");
        if (jerry_value_is_function(activate_func)) {
            // if onactivate exists, call it
#ifdef ZJS_FIND_FUNC_NAME
            zjs_obj_add_string(activate_func, ZJS_HIDDEN_PROP("function_name"),
                               "sensor: onactivate");
#endif
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
    u64_t timestamp = k_uptime_get();
    zjs_obj_add_readonly_number(obj, "timestamp", ((double)timestamp));

    ZVAL func = zjs_get_property(obj, "onchange");
    if (jerry_value_is_function(func)) {
        ZVAL event = zjs_create_object();
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
        ZVAL event = zjs_create_object();
        ZVAL error_obj = zjs_create_object();
        ZVAL name_val = jerry_create_string(error_name);
        ZVAL message_val = jerry_create_string(error_message);
        zjs_set_property(error_obj, "name", name_val);
        zjs_set_property(error_obj, "message", message_val);
        zjs_set_property(event, "error", error_obj);
#ifdef ZJS_FIND_FUNC_NAME
        zjs_obj_add_string(func, ZJS_HIDDEN_PROP("function_name"),
                           "sensor: onerror");
#endif
        zjs_callback_id id = zjs_add_callback_once(func, obj, NULL, NULL);
        zjs_signal_callback(id, &event, sizeof(event));
        DBG_PRINT("triggering error %s (%s)\n", error_name, error_message);
    }
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

    if (zjs_sensor_board_start(handle) != 0) {
        zjs_sensor_trigger_error(this, "SensorError", "start failed");
    }

    zjs_sensor_set_state(this, SENSOR_STATE_ACTIVATED);
    zjs_signal_callback(handle->onstart_cb_id, NULL, 0);
    DBG_PRINT("sensor driver %s started\n", handle->controller->name);
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

    if (zjs_sensor_board_stop(handle) != 0) {
        zjs_sensor_trigger_error(this, "SensorError", "stop failed");
    }

    zjs_sensor_set_state(this, SENSOR_STATE_IDLE);
    zjs_signal_callback(handle->onstop_cb_id, NULL, 0);
    DBG_PRINT("sensor driver %s stopped\n", handle->controller->name);
    return ZJS_UNDEFINED;
}

ZJS_DECL_FUNC_ARGS(zjs_sensor_create,
                   sensor_instance_t *instance,
                   enum sensor_channel channel,
                   const char *c_name,
                   u32_t pin,
                   u32_t max_frequency,
                   zjs_c_callback_func onchange,
                   zjs_c_callback_func onstart,
                   zjs_c_callback_func onstop)
{
    if (!instance) {
        return zjs_error("sensor instance not found");
    }

    double frequency = DEFAULT_SAMPLING_FREQUENCY;
    jerry_size_t size = SENSOR_MAX_CONTROLLER_NAME_LEN;
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

        u32_t option_pin;
#ifdef BUILD_MODULE_AIO
        ZVAL pin_val = zjs_get_property(options, "pin");
        if (jerry_value_is_string(pin_val)) {
            char devname[20];
            option_pin = zjs_board_find_aio(pin_val, devname, 20);
            if (option_pin == FIND_PIN_INVALID) {
                return TYPE_ERROR("bad pin argument");
            } else if (option_pin == FIND_DEVICE_FAILURE) {
                return zjs_error("device not found");
            } else if (option_pin < 0) {
                return zjs_error("pin not found");
            }
#else
        if (zjs_obj_get_uint32(options, "pin", &option_pin)) {
#endif
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

    // FIXME: Why not allocate controller earlier and write to it initially
    //   above? Furthermore, why not put it directly in handle instead of
    //   allocating a separate block of memory?
    sensor_handle_t *handle = zjs_sensor_alloc_handle(&(instance->handles));
    handle->controller = zjs_malloc(sizeof(sensor_controller_t));
    *handle->controller = controller;
    handle->channel = channel;
    handle->frequency = frequency;

    if (zjs_sensor_board_create(handle) != 0) {
        zjs_free(handle->controller);
        zjs_free(handle);
        return zjs_error("zjs_sensor_create failed to init\n");
    }

    // initialize object and default values
    jerry_value_t sensor_obj = zjs_create_object();
    ZVAL null_val = jerry_create_null();

    zjs_set_readonly_property(sensor_obj, "timestamp", null_val);
    zjs_obj_add_number(sensor_obj, "frequency", frequency);
    zjs_obj_add_readonly_string(sensor_obj, "state", "unconnected");
    jerry_set_prototype(sensor_obj, zjs_sensor_prototype);

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
    zjs_native_func_t array[] = {
        { zjs_sensor_start, "start" },
        { zjs_sensor_stop, "stop" },
        { NULL, NULL }
    };

    zjs_sensor_board_init();
    zjs_sensor_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_sensor_prototype, array);
}

void zjs_sensor_cleanup()
{
    zjs_sensor_board_cleanup();
    jerry_release_value(zjs_sensor_prototype);
}

#endif  // QEMU_BUILD
