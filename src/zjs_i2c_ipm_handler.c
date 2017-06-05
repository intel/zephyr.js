// Copyright (c) 2016-2017, Intel Corporation.
#include "zjs_i2c_handler.h"
#include "zjs_ipm.h"
#include "zjs_common.h"
#include <string.h>

static struct k_sem i2c_sem;
#define ZJS_I2C_TIMEOUT_TICKS                      5000

static bool zjs_i2c_ipm_send_sync(zjs_ipm_message_t *send,
                                  zjs_ipm_message_t *result) {
    send->id = MSG_ID_I2C;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_I2C, send) != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    // block until reply or timeout
    if (k_sem_take(&i2c_sem, ZJS_I2C_TIMEOUT_TICKS)) {
        ERR_PRINT("ipm timed out\n");
        return false;
    }

    return true;
}

static void ipm_msg_receive_callback(void *context, u32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_I2C)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);

    if (msg->flags & MSG_SYNC_FLAG) {
         zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;
        // synchronous ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(zjs_ipm_message_t));

        // un-block sync api
        k_sem_give(&i2c_sem);
    } else {
        // asynchronous ipm, should not get here
        ERR_PRINT("async message received\n");
    }
}

void zjs_i2c_ipm_init() {
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_I2C, ipm_msg_receive_callback);
    k_sem_init(&i2c_sem, 0, 1);
}

int zjs_i2c_handle_write(u8_t msg_bus, u8_t *data, u32_t length, u16_t address)
{
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_WRITE;
    send.data.i2c.bus = (u8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (u16_t)address;
    send.data.i2c.length = length;


    // We have ARC send it via ipm
    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        ERR_PRINT("ipm message failed or timed out!\n");
    }

    return send.error_code;
}

int zjs_i2c_handle_open(u8_t msg_bus)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_OPEN;
    send.data.i2c.bus = (u8_t)msg_bus;
    send.data.i2c.speed = (u8_t)I2C_SPEED_STANDARD;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        ERR_PRINT("ipm message failed or timed out!\n");
    }

    if (send.error_code) {
        ERR_PRINT("Failed to open I2C bus\n");
    }

    return send.error_code;
}

int zjs_i2c_handle_read(u8_t msg_bus, u8_t *data, u32_t length, u16_t address)
{
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_READ;
    send.data.i2c.bus = (u8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (u16_t)address;
    send.data.i2c.length = length;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        ERR_PRINT("ipm message failed or timed out!\n");
    }
    return send.error_code;
}

int zjs_i2c_handle_burst_read(u8_t msg_bus, u8_t *data, u32_t length,
                              u16_t address, u16_t register_addr)
{
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_BURST_READ;
    send.data.i2c.bus = (u8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (u16_t)address;
    send.data.i2c.register_addr = register_addr;
    send.data.i2c.length = length;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        ERR_PRINT("ipm message failed or timed out!\n");
    }
    return send.error_code;
}
