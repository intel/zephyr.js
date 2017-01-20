// Copyright (c) 2016-2017, Intel Corporation.
#include "zjs_i2c_handler.h"
#include "zjs_ipm.h"
#include "zjs_common.h"
#include "jerry-api.h"
#include "zjs_error.h"

static struct k_sem i2c_sem;
#define ZJS_I2C_TIMEOUT_TICKS                      5000

static bool zjs_i2c_ipm_send_sync(zjs_ipm_message_t* send,
                                  zjs_ipm_message_t* result) {
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

static void ipm_msg_receive_callback(void *context, uint32_t id,
                                     volatile void *data)
{
    if (id != MSG_ID_I2C)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t*)(*(uintptr_t *)data);

    if (msg->flags & MSG_SYNC_FLAG) {
         zjs_ipm_message_t *result = (zjs_ipm_message_t*)msg->user_data;
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

uint8_t zjs_i2c_handle_write (uint8_t msg_bus, uint8_t *data, uint32_t length,
                              uint16_t address)
{
    uint8_t error_code = 0;
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_WRITE;
    send.data.i2c.bus = (uint8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (uint16_t)address;
    send.data.i2c.length = length;


    // We have ARC send it via ipm
    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_write: ipm message failed or timed out!");
    }

    return send.error_code;
}

uint8_t zjs_i2c_handle_open (uint8_t msg_bus)
{
    uint8_t error_code = 0;
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_OPEN;
    send.data.i2c.bus = (uint8_t)msg_bus;
    send.data.i2c.speed = (uint8_t)I2C_SPEED_STANDARD;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_write: ipm message failed or timed out!");
    }

    if (reply.error_code) {
        return zjs_error("zjs_i2c_open: Failed to open I2C bus");
    }

    return error_code;
}

uint8_t zjs_i2c_handle_read (uint8_t msg_bus, uint8_t *data, uint32_t length,
                             uint16_t address)
{
    uint8_t error_code = 0;
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_READ;
    send.data.i2c.bus = (uint8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (uint16_t)address;
    send.data.i2c.length = length;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_read_base: ipm message failed or timed out!");
    }
    return error_code;
}

uint8_t zjs_i2c_handle_burst_read (uint8_t msg_bus, uint8_t *data,
                                   uint32_t length, uint16_t address,
                                   uint16_t register_addr)
{
    uint8_t error_code = 0;
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_BURST_READ;
    send.data.i2c.bus = (uint8_t)msg_bus;
    send.data.i2c.data = data;
    send.data.i2c.address = (uint16_t)address;
    send.data.i2c.register_addr = register_addr;
    send.data.i2c.length = length;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_read_base: ipm message failed or timed out!");
    }
    return error_code;
}
