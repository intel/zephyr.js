// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
// ipm for ARC communication
#include <ipm.h>
#include <ipm/ipm_quark_se.h>
#include <misc/util.h>
#include <string.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

// ZJS includes
#include "zjs_aio.h"
#include "zjs_util.h"

QUARK_SE_IPM_DEFINE(temperature_ipm, 0, QUARK_SE_IPM_INBOUND);

#define IPM_DRV_NAME "temperature_ipm"

static uint8_t temperature = 0;
static struct device *zjs_ipm_dev;

void temperature_ipm_callback(void *context, uint32_t id, volatile void *data)
{
    temperature = *(uint8_t*) data;
}

jerry_api_object_t *zjs_aio_init()
{
    zjs_ipm_dev = device_get_binding(IPM_DRV_NAME);

    if (!zjs_ipm_dev) {
        PRINT("Cannot find %s!\n", IPM_DRV_NAME);
        return NULL;
    }

    ipm_register_callback(zjs_ipm_dev, temperature_ipm_callback, NULL);
    ipm_set_enabled(zjs_ipm_dev, 1);

    // create global AIO object
    jerry_api_object_t *aio_obj = jerry_api_create_object();
    zjs_obj_add_function(aio_obj, zjs_aio_open, "open");
    return aio_obj;
}

bool zjs_aio_open(const jerry_api_object_t *function_obj_p,
                  const jerry_api_value_t *this_p,
                  jerry_api_value_t *ret_val_p,
                  const jerry_api_value_t args_p[],
                  const jerry_api_length_t args_cnt)
{
    if (args_cnt < 1 ||
        args_p[0].type != JERRY_API_DATA_TYPE_OBJECT)
    {
        PRINT("zjs_aio_open: invalid arguments\n");
        return false;
    }

    jerry_api_object_t *data = args_p[0].u.v_object;

    uint32_t device;
    if (!zjs_obj_get_uint32(data, "device", &device)) {
        PRINT("zjs_aio_open: missing required field (device)\n");
        return false;
    }

    uint32_t pin;
    if (!zjs_obj_get_uint32(data, "pin", &pin)) {
        PRINT("zjs_aio_open: missing required field (pin)\n");
        return false;
    }

    const int BUFLEN = 32;
    char buffer[BUFLEN];

    if (zjs_obj_get_string(data, "name", buffer, BUFLEN)) {
        buffer[0] = '\0';
    }

    char *name = buffer;
    bool raw = false;
    zjs_obj_get_boolean(data, "raw", &raw);

    // create the AIOPin object
    jerry_api_object_t *pinobj = jerry_api_create_object();
    zjs_obj_add_function(pinobj, zjs_aio_pin_read, "read");
    zjs_obj_add_function(pinobj, zjs_aio_pin_abort, "abort");
    zjs_obj_add_function(pinobj, zjs_aio_pin_close, "close");
    zjs_obj_add_string(pinobj, name, "name");
    zjs_obj_add_uint32(pinobj, device, "device");
    zjs_obj_add_uint32(pinobj, pin, "pin");
    zjs_obj_add_boolean(pinobj, raw, "raw");

    *ret_val_p = jerry_api_create_object_value(pinobj);
    return true;
}

bool zjs_aio_pin_read(const jerry_api_object_t *function_obj_p,
                      const jerry_api_value_t *this_p,
                      jerry_api_value_t *ret_val_p,
                      const jerry_api_value_t args_p[],
                      const jerry_api_length_t args_cnt)
{
    jerry_api_object_t *obj = jerry_api_get_object_value(this_p);

    uint32_t device, pin;
    zjs_obj_get_uint32(obj, "device", &device);
    zjs_obj_get_uint32(obj, "pin", &pin);


    double value = (double) temperature;
    if (value <= 0) {
        PRINT("error: reading from ARC pin device %lu pin #%lu\n", device, pin);
        return false;
    }

    ret_val_p->type = JERRY_API_DATA_TYPE_FLOAT64;
    ret_val_p->u.v_float64 = value;

    return true;
}

bool zjs_aio_pin_abort(const jerry_api_object_t *function_obj_p,
                       const jerry_api_value_t *this_p,
                       jerry_api_value_t *ret_val_p,
                       const jerry_api_value_t args_p[],
                       const jerry_api_length_t args_cnt)
{
    // NO-OP
    return true;
}

bool zjs_aio_pin_close(const jerry_api_object_t *function_obj_p,
                       const jerry_api_value_t *this_p,
                       jerry_api_value_t *ret_val_p,
                       const jerry_api_value_t args_p[],
                       const jerry_api_length_t args_cnt)
{
    // NO-OP
    return true;
}
