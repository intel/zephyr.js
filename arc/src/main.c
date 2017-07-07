// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

// Zephyr includes
#include <zephyr.h>
#ifdef BUILD_MODULE_AIO
#include <adc.h>
#endif

// ZJS includes
#include "arc_common.h"
#ifdef BUILD_MODULE_AIO
#include "arc_aio.h"
#endif
#ifdef BUILD_MODULE_GROVE_LCD
#include "arc_grove_lcd.h"
#endif
#ifdef BUILD_MODULE_I2C
#include "arc_i2c.h"
#endif
#ifdef BUILD_MODULE_PME
#include "arc_pme.h"
#endif
#ifdef BUILD_MODULE_SENSOR
#include "arc_sensor.h"
#endif

#define AIO_UPDATE_INTERVAL 200  // 2sec interval in between notifications
#define QUEUE_SIZE 10            // max incoming message can handle
#define SLEEP_TICKS 1            // 10ms sleep time in cpu ticks

static struct k_sem arc_sem;

#ifdef CONFIG_IPM
static struct zjs_ipm_message msg_queue[QUEUE_SIZE];
static struct zjs_ipm_message *end_of_queue_ptr = msg_queue + QUEUE_SIZE;

int ipm_send_msg(struct zjs_ipm_message *msg)
{
    msg->flags &= ~MSG_ERROR_FLAG;
    msg->error_code = ERROR_IPM_NONE;
    return zjs_ipm_send(msg->id, msg);
}

int ipm_send_error(struct zjs_ipm_message *msg, u32_t error_code)
{
    msg->flags |= MSG_ERROR_FLAG;
    msg->error_code = error_code;
    DBG_PRINT("send error %u\n", msg->error_code);
    return zjs_ipm_send(msg->id, msg);
}

static void queue_message(struct zjs_ipm_message *incoming_msg)
{
    struct zjs_ipm_message *msg = msg_queue;

    if (!incoming_msg) {
        return;
    }

    k_sem_take(&arc_sem, K_FOREVER);
    while (msg && msg < end_of_queue_ptr) {
        if (msg->id == MSG_ID_DONE) {
            break;
        }
        msg++;
    }

    if (msg != end_of_queue_ptr) {
        // copy the message into our queue to be process in the mainloop
        memcpy(msg, incoming_msg, sizeof(struct zjs_ipm_message));
    } else {
        // running out of space, disregard message
        ERR_PRINT("skipping incoming message\n");
    }
    k_sem_give(&arc_sem);
}

static void ipm_msg_receive_callback(void *context, u32_t id,
                                     volatile void *data)
{
    struct zjs_ipm_message *incoming_msg =
        (struct zjs_ipm_message *)(*(uintptr_t *)data);
    if (incoming_msg) {
        queue_message(incoming_msg);
    } else {
        ERR_PRINT("message is NULL\n");
    }
}

static void process_messages()
{
    struct zjs_ipm_message *msg = msg_queue;

    while (msg && msg < end_of_queue_ptr) {
        // loop through all messages and process them
        switch (msg->id) {
#ifdef BUILD_MODULE_AIO
        case MSG_ID_AIO:
            arc_handle_aio(msg);
            break;
#endif
#ifdef BUILD_MODULE_I2C
        case MSG_ID_I2C:
            arc_handle_i2c(msg);
            break;
#endif
#ifdef BUILD_MODULE_GROVE_LCD
        case MSG_ID_GLCD:
            arc_handle_glcd(msg);
            break;
#endif
#ifdef BUILD_MODULE_SENSOR
        case MSG_ID_SENSOR:
            arc_handle_sensor(msg);
            break;
#endif
#ifdef BUILD_MODULE_PME
        case MSG_ID_PME:
            arc_handle_pme(msg);
            break;
#endif
        case MSG_ID_DONE:
            return;
        default:
            ERR_PRINT("unsupported ipm message id: %u, check ARC modules\n",
                      msg->id);
            ipm_send_error(msg, ERROR_IPM_NOT_SUPPORTED);
        }

        msg->id = MSG_ID_DONE;
        msg++;
    }
}
#endif

void main(void)
{
    ZJS_PRINT("Sensor core running ZJS ARC support image\n");

    k_sem_init(&arc_sem, 0, 1);
    k_sem_give(&arc_sem);
#ifdef CONFIG_IPM
    memset(msg_queue, 0, sizeof(struct zjs_ipm_message) * QUEUE_SIZE);

    zjs_ipm_init();
    zjs_ipm_register_callback(-1, ipm_msg_receive_callback);  // MSG_ID ignored
#endif
#ifdef BUILD_MODULE_AIO
    arc_aio_init();
#endif

    int tick_count = 0;
    while (1) {
#ifdef CONFIG_IPM
        process_messages();
#endif
#ifdef BUILD_MODULE_AIO
        if (tick_count % AIO_UPDATE_INTERVAL == 0) {
            arc_process_aio_updates();
        }
#endif
#ifdef BUILD_MODULE_SENSOR
        if (tick_count % (u32_t)(CONFIG_SYS_CLOCK_TICKS_PER_SEC /
                                 sensor_poll_freq) == 0) {
            arc_fetch_sensor();
#ifdef BUILD_MODULE_SENSOR_LIGHT
            arc_fetch_light();
#endif
        }
#endif
        tick_count += SLEEP_TICKS;
        k_sleep(SLEEP_TICKS);
    }

#ifdef BUILD_MODULE_AIO
    arc_aio_cleanup();
#endif
}
