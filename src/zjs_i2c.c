// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_I2C
#ifndef QEMU_BUILD
// Zephyr includes
#include <i2c.h>
#include <string.h>

// ZJS includes
#include "zjs_i2c.h"
#include "zjs_ipm.h"
#include "zjs_util.h"
#include "zjs_buffer.h"

#define ZJS_I2C_TIMEOUT_TICKS                      500

static struct k_sem i2c_sem;

static bool zjs_i2c_ipm_send_sync(zjs_ipm_message_t* send,
                                  zjs_ipm_message_t* result) {
    send->id = MSG_ID_I2C;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_I2C, send) != 0) {
        ERR_PRINT("zjs_i2c_ipm_send_sync: failed to send message\n");
        return false;
    }

    // block until reply or timeout
    if (k_sem_take(&i2c_sem, ZJS_I2C_TIMEOUT_TICKS)) {
        ERR_PRINT("zjs_i2c_ipm_send_sync: ipm timed out\n");
        return false;
    }

    return true;
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
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
        ERR_PRINT("ipm_msg_receive_callback: async message received\n");
    }
}

static jerry_value_t zjs_i2c_read_base(const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc,
                                       bool                 burst)
{
    // requires: Requires three arguments and has an optional fourth.
    //           arg[0] - Address of the I2C device you wish to read from.
    //           arg[1] - Number of bytes to read
    //           burst  - True if this is a burst read.
    //           arg[2] - Register address you wish to read from on the
    //                    device. (Optional. Default address is 0x00)
    //  effects: Reads the number of bytes requested from the I2C device
    //           from the register address. Returns a buffer object
    //           that size containing the data.

    if (argc < 2 || !jerry_value_is_number(argv[0]) ||
        !jerry_value_is_number(argv[1])) {
        return zjs_error("zjs_i2c_read_base: missing arguments");
    }

    uint32_t register_addr = 0;
    uint32_t size = (uint32_t)jerry_get_number_value(argv[1]);

    if (argc >= 3) {
        if (!jerry_value_is_number(argv[2])) {
            return zjs_error("zjs_i2c_read_base: register is not a number");
        }
        register_addr = (uint32_t)jerry_get_number_value(argv[2]);
    }

    if (size < 1) {
        return zjs_error("zjs_i2c_read_base: size should be greater than zero");
    }

    uint32_t bus;
    zjs_obj_get_uint32(this, "bus", &bus);
    uint32_t address = (uint32_t)jerry_get_number_value(argv[0]);
    jerry_value_t buf_obj = zjs_buffer_create(size);
    zjs_buffer_t *buf;

    if (jerry_value_is_object(buf_obj)) {
        buf = zjs_buffer_find(buf_obj);

        if (!burst && (register_addr != 0)) {
            // i2c_read checks the first byte for the register address
            // i2c_burst_read doesn't
            buf->buffer[0] = (uint8_t)register_addr;
        }
    }
    else {
        return zjs_error("zjs_i2c_read_base: buffer creation failed");
    }

    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    if (!burst) {
        send.type = TYPE_I2C_READ;
    } else {
        send.type = TYPE_I2C_BURST_READ;
    }

    send.data.i2c.bus = (uint8_t)bus;
    send.data.i2c.data = buf->buffer;
    send.data.i2c.address = (uint16_t)address;
    send.data.i2c.register_addr = register_addr;
    send.data.i2c.length = size;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_read_base: ipm message failed or timed out!");
    }

    return buf_obj;
}

static jerry_value_t zjs_i2c_read(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    // requires: Requires two arguments and has an optional third.
    //           arg[0] - Address of the I2C device you wish to read from.
    //           arg[1] - Number of bytes to read
    //           arg[2] - Register address you wish to read from on the
    //                    device. (Optional. Default address is 0x00)
    //  effects: Reads the number of bytes requested from the I2C device
    //           from the register address. Returns a buffer object
    //           that size containing the data.

    return zjs_i2c_read_base(this, argv, argc, false);
}

static jerry_value_t zjs_i2c_burst_read(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    // requires: Requires two arguments and has an optional third.
    //           arg[0] - Address of the I2C device you wish to read from.
    //           arg[1] - Number of bytes to read
    //           arg[2] - Register address you wish to read from on the
    //                    device. (Optional. Default address is 0x00)
    //  effects: This API reads from several address registers in a row,
    //           starting at the register address.
    //           Reads the number of bytes requested from the I2C device.
    //           Returns a buffer object containing the data.

    return zjs_i2c_read_base(this, argv, argc, true);
}

static jerry_value_t zjs_i2c_write(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // requires: Requires two arguments and has an optional third.
    //           arg[0] - Address of the I2C device you wish to write to.
    //           arg[1] - Buffer object containing data you want to write.
    //           arg[2] - Register address you wish to write to on the
    //                    device. (Optional. Default address is 0x00 or
    //                    whatever is stored in the first byte of arg[1])
    //  effects: Writes the number of bytes requested to the I2C device
    //           at the register address. Returns an error if unsuccessful.

    if (argc < 2 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_i2c_write: missing arguments");
    }

    if (!jerry_value_is_object(argv[1])) {
        return zjs_error("zjs_i2c_write: message buffer is invalid");
    }

    uint32_t register_addr = 0;

    if (argc >= 3 && !jerry_value_is_number(argv[2])) {
        return zjs_error("zjs_i2c_read: register is not a number");
    } else {
        register_addr = (uint32_t)jerry_get_number_value(argv[2]);
    }

    uint32_t bus;
    zjs_obj_get_uint32(this, "bus", &bus);
    zjs_buffer_t *dataBuf = zjs_buffer_find(argv[1]);

    if (dataBuf != NULL) {
        if (register_addr != 0) {
            // If the user supplied a register address, add it to the beginning
            // of the buffer, as that's where i2c_write will expect it.
            dataBuf->buffer[0] = (uint8_t)register_addr;
        }
    } else {
        return zjs_error("zjs_i2c_write:  missing data buffer");
    }

    uint32_t address = (uint32_t)jerry_get_number_value(argv[0]);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_WRITE;
    send.data.i2c.bus = (uint8_t)bus;
    send.data.i2c.data = dataBuf->buffer;
    send.data.i2c.register_addr = register_addr;
    send.data.i2c.address = (uint16_t)address;
    send.data.i2c.length = dataBuf->bufsize;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_write: ipm message failed or timed out!");
    }

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_abort(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_close(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_i2c_open(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    // requires: Requires two arguments
    //           arg[0] - I2C bus number you want to open a connection to.
    //           arg[1] - Bus speed in kbps.
    //  effects: Creates a I2C object connected to the bus number specified.

    if (argc < 1 || !jerry_value_is_object(argv[0])) {
        return zjs_error("zjs_i2c_open: invalid argument");
    }

    jerry_value_t data = argv[0];
    uint32_t bus;
    uint32_t speed;

    if (!zjs_obj_get_uint32(data, "bus", &bus)) {
        return zjs_error("zjs_i2c_open: missing required field (bus)");
    }

    if (!zjs_obj_get_uint32(data, "speed", &speed)) {
        return zjs_error("zjs_i2c_open: missing required field (speed)");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    zjs_ipm_message_t reply;

    send.type = TYPE_I2C_OPEN;
    send.data.i2c.bus = (uint8_t)bus;
    send.data.i2c.speed = (uint8_t)speed;

    bool success = zjs_i2c_ipm_send_sync(&send, &reply);

    if (!success) {
        return zjs_error("zjs_i2c_write: ipm message failed or timed out!");
    }

    // create the I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    zjs_obj_add_function(i2c_obj, zjs_i2c_read, "read");
    zjs_obj_add_function(i2c_obj, zjs_i2c_burst_read, "burstRead");
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

    k_sem_init(&i2c_sem, 0, 1);

    // create global I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    zjs_obj_add_function(i2c_obj, zjs_i2c_open, "open");
    return i2c_obj;
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_I2C
