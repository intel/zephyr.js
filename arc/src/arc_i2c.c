// Copyright (c) 2017, Intel Corporation.

// Zephyr includes
#include <i2c.h>
#include <zephyr.h>

// ZJS includes
#include "arc_common.h"
#include "arc_i2c.h"
#include "zjs_i2c_handler.h"

#define MAX_I2C_BUS 2

void arc_handle_i2c(struct zjs_ipm_message *msg)
{
    u32_t error_code = ERROR_IPM_NONE;
    u8_t msg_bus = msg->data.i2c.bus;

    switch (msg->type) {
    case TYPE_I2C_OPEN:
        error_code = zjs_i2c_handle_open(msg_bus);
        break;
    case TYPE_I2C_WRITE:
        error_code = zjs_i2c_handle_write(msg_bus, msg->data.i2c.data,
                                          msg->data.i2c.length,
                                          msg->data.i2c.address);
        break;
    case TYPE_I2C_WRITE_BIT:
        DBG_PRINT("received TYPE_I2C_WRITE_BIT\n");
        break;
    case TYPE_I2C_READ:
        error_code = zjs_i2c_handle_read(msg_bus, msg->data.i2c.data,
                                         msg->data.i2c.length,
                                         msg->data.i2c.address);
        break;
    case TYPE_I2C_BURST_READ:
        error_code = zjs_i2c_handle_burst_read(msg_bus, msg->data.i2c.data,
                                               msg->data.i2c.length,
                                               msg->data.i2c.address,
                                               msg->data.i2c.register_addr);
        break;
    case TYPE_I2C_TRANSFER:
        DBG_PRINT("received TYPE_I2C_TRANSFER\n");
        break;
    default:
        ERR_PRINT("unsupported i2c message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    ipm_send_msg(msg);
}
