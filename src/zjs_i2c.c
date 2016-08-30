// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_I2C
#ifndef QEMU_BUILD
// Zephyr includes
#include <zephyr.h>
#include <i2c.h>
// ZJS includes
#include "zjs_i2c.h"
#include "zjs_ipm.h"
#include "zjs_util.h"
#include "zjs_buffer.h"

static struct nano_sem i2c_sem;

static int zjs_i2c_ipm_send(struct zjs_i2c_ipm_message msg) {

    msg.block = 1;

    return zjs_ipm_send(MSG_ID_I2C, &msg, sizeof(msg));
}


static jerry_value_t zjs_i2c_read(const jerry_value_t function_obj_val,
                                  const jerry_value_t this_val,
                                  const jerry_value_t args_p[],
                                  const jerry_length_t args_cnt)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_write(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    if (args_cnt < 2 || !jerry_value_is_object(args_p[0])) {
        return zjs_error("zjs_i2c_write: missing arguments");
    }

    if (!jerry_value_is_object(args_p[1])) {
        return zjs_error("zjs_i2c_write: message buffer is invalid");
    }

    uint32_t bus;
    zjs_obj_get_uint32(this_val, "bus", &bus);

    jerry_value_t jerryData = args_p[0];

    uint32_t length;
    if (!zjs_obj_get_uint32(jerryData, "length", &length)) {
        return zjs_error("zjs_i2c_write: missing required field (length)");
    }

    struct zjs_buffer_t *dataBuf = zjs_buffer_find(args_p[1]);

    if (!dataBuf) {
        return zjs_error("zjs_i2c_write:  missing data buffer");
    }

    uint32_t address;
    if (!zjs_obj_get_uint32(jerryData, "address", &address)) {
        return zjs_error("zjs_i2c_write: missing required field (address)");
    }

    struct zjs_i2c_ipm_message msg;
    msg.type = TYPE_I2C_WRITE;
    msg.bus = (uint8_t)bus;
    msg.data = dataBuf->buffer;
    msg.address = (uint16_t)address;
    msg.length = dataBuf->bufsize;

    // send IPM message to the ARC side
    zjs_i2c_ipm_send(msg);
    // block until reply or timeout
    if (nano_sem_take(&i2c_sem, 5000) == 0) {
        return zjs_error("zjs_i2c_write: reply from ARC timed out!");
    }

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_abort(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_close(const jerry_value_t function_obj_val,
                                   const jerry_value_t this_val,
                                   const jerry_value_t args_p[],
                                   const jerry_length_t args_cnt)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

// callback for incoming ipm messages from arc
static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_I2C)
        return;

    struct zjs_i2c_ipm_message_reply *msg = (struct zjs_i2c_ipm_message_reply *) data;

    if (msg->type == TYPE_I2C_OPEN_SUCCESS) {
        //PRINT("ipm_msg_receive_callback: I2C is opened\n");
    } else if (msg->type == TYPE_I2C_WRITE_SUCCESS) {
        //PRINT("ipm_msg_receive_callback: I2C write success\n");
    } else if (msg->type == TYPE_I2C_OPEN_FAIL ||
               msg->type == TYPE_I2C_WRITE_FAIL) {
        PRINT("ipm_msg_receive_callback: failed to perform operation %u\n", msg->type);
    } else {
        PRINT("ipm_msg_receive_callback: IPM message not handled %u\n", msg->type);
    }

    if (msg->block) {
        // un-block sync api
        nano_isr_sem_give(&i2c_sem);
    }
}

static jerry_value_t zjs_i2c_open(const jerry_value_t function_obj_val,
                                  const jerry_value_t this_val,
                                  const jerry_value_t args_p[],
                                  const jerry_length_t args_cnt)
{

    if (args_cnt < 1 || !jerry_value_is_object(args_p[0])) {
        return zjs_error("zjs_i2c_open: invalid argument");
    }

    jerry_value_t data = args_p[0];

    uint32_t bus;
    if (!zjs_obj_get_uint32(data, "bus", &bus)) {
        return zjs_error("zjs_i2c_open: missing required field (bus)");
    }

    uint32_t speed;
    if (!zjs_obj_get_uint32(data, "speed", &speed)) {
        return zjs_error("zjs_i2c_open: missing required field (speed)");
    }

    struct zjs_i2c_ipm_message msg;
    msg.type = TYPE_I2C_OPEN;
    msg.bus = (uint8_t)bus;
    msg.speed = (uint8_t)speed;

    // send IPM message to the ARC side
    zjs_i2c_ipm_send(msg);
    // block until reply or timeout

    if (nano_sem_take(&i2c_sem, 5000) == 0) {
        return zjs_error("zjs_i2c_open: reply from ARC timed out!\n");
    }

    // create the I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    zjs_obj_add_function(i2c_obj, zjs_i2c_read, "read");
    zjs_obj_add_function(i2c_obj, zjs_i2c_write, "write");
    zjs_obj_add_function(i2c_obj, zjs_i2c_abort, "abort");
    zjs_obj_add_function(i2c_obj, zjs_i2c_close, "close");
    zjs_obj_add_number(i2c_obj, bus, "bus");
    zjs_obj_add_number(i2c_obj, speed, "speed");

    return i2c_obj;
}

jerry_value_t zjs_i2c_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_I2C, ipm_msg_receive_callback);

    nano_sem_init(&i2c_sem);

    // create global I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    zjs_obj_add_function(i2c_obj, zjs_i2c_open, "open");
    return i2c_obj;
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_I2C
