// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include "zjs_common.h"
#include "zjs_modules.h"
#include "zjs_sensor_board.h"
#include "zjs_util.h"

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
#ifdef BUILD_MODULE_SENSOR_MAGN
#include "zjs_sensor_magn.h"
#endif

typedef sensor_instance_t *(*initcb_t)();
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
#ifdef BUILD_MODULE_SENSOR_MAGN
    { SENSOR_CHAN_MAGN_XYZ, zjs_sensor_magn_init, zjs_sensor_magn_cleanup },
#endif
};

int zjs_sensor_board_start(sensor_handle_t *handle)
{
    if (!handle->controller->dev) {
        ERR_PRINT("dev %s not found\n", handle->controller->name);
    }

    switch (handle->channel) {
    case SENSOR_CHAN_ACCEL_XYZ:
    case SENSOR_CHAN_GYRO_XYZ:
    case SENSOR_CHAN_LIGHT:
    case SENSOR_CHAN_MAGN_XYZ:
    case SENSOR_CHAN_TEMP:
        break;

    default:
        ERR_PRINT("unsupported sensor channel\n");
        return -1;
    }

    return 0;
}

int zjs_sensor_board_stop(sensor_handle_t *handle)
{
    if (!handle->controller->dev) {
        ERR_PRINT("dev %s not found\n", handle->controller->name);
    }

    switch (handle->channel) {
    case SENSOR_CHAN_ACCEL_XYZ:
    case SENSOR_CHAN_GYRO_XYZ:
    case SENSOR_CHAN_LIGHT:
    case SENSOR_CHAN_MAGN_XYZ:
    case SENSOR_CHAN_TEMP:
        break;

    default:
        ERR_PRINT("unsupported sensor channel\n");
        return -1;
    }

    return 0;
}

int zjs_sensor_board_create(sensor_handle_t *handle)
{
    if (!handle || !handle->controller)
        return -1;

    handle->controller->dev = device_get_binding(handle->controller->name);
    if (!handle->controller->dev) {
        ERR_PRINT("failed to open dev %s\n", handle->controller->name);
        return -1;
    }

    ZJS_PRINT("handle->controller->name %s\n", handle->controller->name);

    return 0;
}

static void zjs_sensor_fetch_sample(sensor_handle_t *handle)
{
    if (sensor_sample_fetch(handle->controller->dev) < 0) {
        ERR_PRINT("sensor_sample_fetch failed\n");
        return;
    }

    struct sensor_value val[3];
    if (handle->channel == SENSOR_CHAN_ACCEL_XYZ) {
        if (sensor_channel_get(handle->controller->dev,
                               handle->channel, val) < 0) {
            ERR_PRINT("failed to read accelerometer channel\n");
            return;
        }
    } else if (handle->channel == SENSOR_CHAN_MAGN_XYZ) {
        if (sensor_channel_get(handle->controller->dev,
                               handle->channel, val) < 0) {
            ERR_PRINT("failed to read magnetometer channel\n");
            return;
        }
    }

    double reading[3];
    reading[0] = sensor_value_to_double(&val[0]);
    reading[1] = sensor_value_to_double(&val[1]);
    reading[2] = sensor_value_to_double(&val[2]);

    sensor_handle_t *h = handle;
    while (h) {
        if (h->state == SENSOR_STATE_ACTIVATED) {
            zjs_signal_callback(h->onchange_cb_id, reading, sizeof(reading));
        }
        h = h->next;
    }
}

static s32_t zjs_sensor_poll_routine(void *h)
{
    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    u32_t uptime = k_uptime_get_32();

    for (int i = 0; i < modcount; i++) {
        sensor_module_t *mod = &sensor_modules[i];
        if (mod->instance && mod->instance->handles) {
            sensor_handle_t *handle = mod->instance->handles;
            if (uptime % (u32_t)(CONFIG_SYS_CLOCK_TICKS_PER_SEC /
                                 handle->frequency * 10) == 0) {
                zjs_sensor_fetch_sample(handle);
            }
        }
    }

    zjs_loop_unblock();
    return K_FOREVER;
}

void zjs_sensor_board_init()
{
    zjs_register_service_routine(NULL, zjs_sensor_poll_routine);

    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    for (int i = 0; i < modcount; i++) {
        sensor_module_t *mod = &sensor_modules[i];
        if (!mod->instance) {
            mod->instance = mod->init();
        }
    }
}

void zjs_sensor_board_cleanup()
{
    int modcount = sizeof(sensor_modules) / sizeof(sensor_module_t);
    for (int i = 0; i < modcount; i++) {
        sensor_module_t *mod = &sensor_modules[i];
        if (mod->instance) {
            mod->cleanup();
        }
        mod->instance = NULL;
    }
    zjs_unregister_service_routine(zjs_sensor_poll_routine);
}
