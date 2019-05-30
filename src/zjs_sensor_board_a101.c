// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <zephyr.h>

// ZJS includes
#include "zjs_common.h"
#include "zjs_ipm.h"
#include "zjs_sensor_board.h"

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
    { SENSOR_CHAN_AMBIENT_TEMP, zjs_sensor_temp_init, zjs_sensor_temp_cleanup },
#endif
};

#define CALL_REMOTE_FUNCTION(msg_type)                                 \
    ({                                                                 \
        zjs_ipm_message_t send, reply;                                 \
        send.type = msg_type;                                          \
        send.data.sensor.channel = handle->channel;                    \
        if (handle->controller) {                                      \
            send.data.sensor.controller = handle->controller->name;    \
            send.data.sensor.pin = handle->controller->pin;            \
            send.data.sensor.frequency = handle->frequency;            \
        }                                                              \
        if (!zjs_sensor_ipm_send_sync(&send, &reply)) {                \
            ERR_PRINT("ipm message failed or timed out!\n");           \
            return -1;                                                 \
        }                                                              \
        if (reply.error_code != ERROR_IPM_NONE) {                      \
            if (reply.error_code == ERROR_IPM_OPERATION_NOT_ALLOWED) { \
                ERR_PRINT("IPM failed - operation not allowed\n");     \
            } else {                                                   \
                ERR_PRINT("IPM failed - unknown error occured\n");     \
            }                                                          \
            return -1;                                                 \
        }                                                              \
        return 0;                                                      \
    })

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
                    zjs_signal_callback(h->onchange_cb_id, &data->reading,
                                        sizeof(data->reading));
                }
            }
            return;
        }
    }

    ERR_PRINT("unsupported sensor type\n");
}

// INTERRUPT SAFE FUNCTION: No JerryScript VM, allocs, or likely prints!
static void ipm_msg_receive_callback(void *context, u32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_SENSOR)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
        zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;

        // synchronous ipm, copy the results
        if (result) {
            *result = *msg;
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

int zjs_sensor_board_start(sensor_handle_t *handle)
{
    CALL_REMOTE_FUNCTION(TYPE_SENSOR_START);
}

int zjs_sensor_board_stop(sensor_handle_t *handle)
{
    CALL_REMOTE_FUNCTION(TYPE_SENSOR_STOP);
}

int zjs_sensor_board_create(sensor_handle_t *handle)
{
    CALL_REMOTE_FUNCTION(TYPE_SENSOR_INIT);
}

void zjs_sensor_board_init()
{
    k_sem_init(&sensor_sem, 0, 1);
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_SENSOR, ipm_msg_receive_callback);

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
}
