// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_common.h"
#include "zjs_sensor.h"
#include "zjs_sensor_temp.h"
#include "zjs_util.h"

static sensor_instance_t *g_instance = NULL;

static void onchange(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    double d;

    // reading is a ptr to double
    d = *((double *)argv);
    zjs_obj_add_readonly_number(obj, "celsius", d);
    zjs_sensor_trigger_change(obj);
}

static void onstop(void *h, const void *argv)
{
    sensor_handle_t *handle = (sensor_handle_t *)h;
    jerry_value_t obj = handle->sensor_obj;

    jerry_value_t null_val = jerry_create_null();
    zjs_set_readonly_property(obj, "illuminance", null_val);
}

static ZJS_DECL_FUNC(zjs_sensor_constructor)
{
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_OBJECT);

    jerry_value_t sensor_obj =
        ZJS_CHAIN_FUNC_ARGS(zjs_sensor_create, g_instance, SENSOR_CHAN_AMBIENT_TEMP,
                            TEMP_DEVICE_NAME, 0, 800, onchange, NULL, onstop);

    if (!jerry_value_is_error(sensor_obj)) {
        ZVAL null_val = jerry_create_null();
        zjs_set_readonly_property(sensor_obj, "celsius", null_val);
    }

    return sensor_obj;
}

sensor_instance_t *zjs_sensor_temp_init()
{
    if (g_instance)
        return g_instance;

    g_instance = zjs_sensor_create_instance("TemperatureSensor",
                                            zjs_sensor_constructor);
    return g_instance;
}

void zjs_sensor_temp_cleanup()
{
    if (g_instance) {
        zjs_sensor_free_instance(g_instance);
        g_instance = NULL;
    }
}
