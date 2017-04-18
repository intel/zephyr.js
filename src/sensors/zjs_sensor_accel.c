// Copyright (c) 2017, Intel Corporation.

#ifdef BUILD_MODULE_SENSOR_ACCEL

#include <string.h>
#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_sensor.h"
#include "zjs_sensor_accel.h"

static sensor_instance_t *g_instance = NULL;

static void onchange(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    double x, y, z;

    // reading is a ptr to an array of 3 double values
    x = ((double *)argv)[0];
    y = ((double *)argv)[1];
    z = ((double *)argv)[2];
    zjs_obj_add_readonly_number(obj, x, "x");
    zjs_obj_add_readonly_number(obj, y, "y");
    zjs_obj_add_readonly_number(obj, z, "z");
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
    zjs_set_readonly_property(obj, "x", null_val);
    zjs_set_readonly_property(obj, "y", null_val);
    zjs_set_readonly_property(obj, "z", null_val);

    if (jerry_value_has_error_flag(zjs_sensor_stop_sensor(obj))) {
        zjs_sensor_trigger_error(obj, "SensorError", "stop failed");
    }
}

static ZJS_DECL_FUNC(zjs_sensor_constructor)
{
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_OBJECT);

    double frequency = DEFAULT_SAMPLING_FREQUENCY;
    size_t size = SENSOR_MAX_CONTROLLER_NAME_LEN;
    sensor_controller_t controller;

    if (argc >= 1) {
        jerry_value_t options = argv[0];
        ZVAL controller_val = zjs_get_property(options, "controller");

        if (jerry_value_is_string(controller_val)) {
            zjs_copy_jstring(controller_val, controller.name, &size);
        } else {
            snprintf(controller.name, size, BMI160_DEVICE_NAME);
            DBG_PRINT("controller not set, default to %s\n", controller.name);
        }

        double option_freq;
        if (zjs_obj_get_double(options, "frequency", &option_freq)) {
            // limit frequency to 800
            if (option_freq < 1 || option_freq > 800) {
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

    jerry_value_t sensor_obj = zjs_sensor_create(g_instance,
                                                 SENSOR_CHAN_ACCEL_XYZ,
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
    zjs_set_readonly_property(sensor_obj, "x", null_val);
    zjs_set_readonly_property(sensor_obj, "y", null_val);
    zjs_set_readonly_property(sensor_obj, "z", null_val);

    return sensor_obj;
}

sensor_instance_t *zjs_sensor_accel_init()
{
    if (g_instance)
        return g_instance;

    ZVAL global_obj = jerry_get_global_object();
    zjs_obj_add_function(global_obj, zjs_sensor_constructor, "Accelerometer");
    g_instance = zjs_malloc(sizeof(sensor_instance_t));
    if (!g_instance) {
        ERR_PRINT("failed to allocate sensor_instance_t\n");
        return NULL;
    }
    memset(g_instance, 0, sizeof(sensor_instance_t));
    return g_instance;
}

void zjs_sensor_accel_cleanup()
{
    if (g_instance) {
        zjs_free(g_instance);
        g_instance = NULL;
    }
}

#endif // BUILD_MODULE_SENSOR_ACCEL