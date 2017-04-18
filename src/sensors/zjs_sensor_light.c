// Copyright (c) 2017, Intel Corporation.

#ifdef BUILD_MODULE_SENSOR_LIGHT

#include <string.h>
#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_sensor.h"
#include "zjs_sensor_light.h"

static sensor_instance_t *g_instance = NULL;

static void onchange(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    double d;

    // reading is a ptr to double
    d = *((double *)argv);
    zjs_obj_add_readonly_number(obj, d, "illuminance");
    zjs_sensor_trigger_change(obj);
}

static void onstart(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    if (jerry_value_has_error_flag(zjs_sensor_start_sensor(obj))) {
        zjs_sensor_trigger_error(obj, "SensorError", "start failed");
    }
}

static void onstop(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    jerry_value_t null_val = jerry_create_null();
    zjs_set_readonly_property(obj, "illuminance", null_val);

    if (jerry_value_has_error_flag(zjs_sensor_stop_sensor(obj))) {
        zjs_sensor_trigger_error(obj, "SensorError", "stop failed");
    }
}

static ZJS_DECL_FUNC(zjs_sensor_constructor)
{
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    double frequency = DEFAULT_SAMPLING_FREQUENCY;
    size_t size = SENSOR_MAX_CONTROLLER_NAME_LEN;
    sensor_controller_t controller;
    jerry_value_t options = argv[0];

    // AmbientLight expectes a obj with pin number
    uint32_t pin;
    if (zjs_obj_get_uint32(options, "pin", &pin)) {
        controller.pin = pin;
    } else {
        ERR_PRINT("pin not found\n");
        return jerry_create_null();
    }

    ZVAL controller_val = zjs_get_property(options, "controller");

    if (jerry_value_is_string(controller_val)) {
        zjs_copy_jstring(controller_val, controller.name, &size);
    } else {
        snprintf(controller.name, size, ADC_DEVICE_NAME);
        DBG_PRINT("controller not set, default to %s\n", controller.name);
    }

    double option_freq;
    if (zjs_obj_get_double(options, "frequency", &option_freq)) {
        // limit frequency to 100
        if (option_freq < 1 || option_freq > 100) {
            ERR_PRINT("unsupported frequency, default to %dHz\n",
                      DEFAULT_SAMPLING_FREQUENCY);
        } else {
            frequency = option_freq;
        }
    } else {
        DBG_PRINT("frequency not found, default to %dHz\n",
                  DEFAULT_SAMPLING_FREQUENCY);
    }

    jerry_value_t sensor_obj = zjs_sensor_create(g_instance,
                                                 SENSOR_CHAN_LIGHT,
                                                 &controller,
                                                 frequency,
                                                 onchange,
                                                 onstart,
                                                 onstop);

    if (jerry_value_has_error_flag(sensor_obj)) {
        return sensor_obj;
    }

    // initialize readings to null
    ZVAL null_val = jerry_create_null();
    zjs_set_readonly_property(sensor_obj, "illuminance", null_val);

    return sensor_obj;
}

sensor_instance_t *zjs_sensor_light_init()
{
    if (g_instance)
        return g_instance;

    ZVAL global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_sensor_constructor, "AmbientLightSensor");
    g_instance = zjs_malloc(sizeof(sensor_instance_t));
    if (!g_instance) {
        ERR_PRINT("failed to allocate sensor_instance_t\n");
        return NULL;
    }
    memset(g_instance, 0, sizeof(sensor_instance_t));
    return g_instance;
}

void zjs_sensor_light_cleanup()
{
    if (g_instance) {
        zjs_free(g_instance);
        g_instance = NULL;
    }
}

#endif // BUILD_MODULE_SENSOR_LIGHT