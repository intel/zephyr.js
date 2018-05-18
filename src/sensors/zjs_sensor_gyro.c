// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_common.h"
#include "zjs_sensor.h"
#include "zjs_sensor_gyro.h"
#include "zjs_util.h"

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
    zjs_obj_add_readonly_number(obj, "x", x);
    zjs_obj_add_readonly_number(obj, "y", y);
    zjs_obj_add_readonly_number(obj, "z", z);
    zjs_sensor_trigger_change(obj);
}

static void onstop(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    jerry_value_t null_val = jerry_create_null();
    zjs_set_readonly_property(obj, "x", null_val);
    zjs_set_readonly_property(obj, "y", null_val);
    zjs_set_readonly_property(obj, "z", null_val);
}

static ZJS_DECL_FUNC(zjs_sensor_constructor)
{
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_OBJECT);

    jerry_value_t sensor_obj =
        ZJS_CHAIN_FUNC_ARGS(zjs_sensor_create, g_instance, SENSOR_CHAN_GYRO_XYZ,
                            GYRO_DEVICE_NAME, 0, 800, onchange, NULL, onstop);

    if (!jerry_value_is_error(sensor_obj)) {
        ZVAL null_val = jerry_create_null();
        zjs_set_readonly_property(sensor_obj, "x", null_val);
        zjs_set_readonly_property(sensor_obj, "y", null_val);
        zjs_set_readonly_property(sensor_obj, "z", null_val);
    }

    return sensor_obj;
}

sensor_instance_t *zjs_sensor_gyro_init()
{
    if (g_instance)
        return g_instance;

    g_instance = zjs_sensor_create_instance("Gyroscope",
                                            zjs_sensor_constructor);
    return g_instance;
}

void zjs_sensor_gyro_cleanup()
{
    if (g_instance) {
        zjs_sensor_free_instance(g_instance);
        g_instance = NULL;
    }
}
