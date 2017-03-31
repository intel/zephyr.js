// Copyright (c) 2016-2017, Intel Corporation.

#ifndef QEMU_BUILD

// Zephyr includes
#include <i2c.h>
#include <string.h>

// ZJS includes
#include "zjs_i2c.h"
#include "zjs_util.h"
#include "zjs_buffer.h"
#include "zjs_i2c_handler.h"

#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_ipm.h"
#include "zjs_i2c_ipm_handler.h"
#endif

static jerry_value_t zjs_i2c_prototype;

static jerry_value_t zjs_i2c_read_base(const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc,
                                       bool burst)
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

    // args: device, length[, register]
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_OPTIONAL Z_NUMBER);

    uint32_t register_addr = 0;
    uint32_t size = (uint32_t)jerry_get_number_value(argv[1]);

    if (argc >= 3) {
        register_addr = (uint32_t)jerry_get_number_value(argv[2]);
    }

    if (size < 1) {
        return zjs_error("zjs_i2c_read_base: size should be greater than zero");
    }

    uint32_t bus;
    zjs_obj_get_uint32(this, "bus", &bus);
    uint32_t address = (uint32_t)jerry_get_number_value(argv[0]);
    zjs_buffer_t *buf;
    jerry_value_t buf_obj = zjs_buffer_create(size, &buf);
    if (buf) {
        if (!burst) {
            if (register_addr != 0) {
                // i2c_read checks the first byte for the register address
                // i2c_burst_read doesn't
                buf->buffer[0] = (uint8_t)register_addr;
            }
            int error_msg = zjs_i2c_handle_read((uint8_t)bus,
                                                buf->buffer,
                                                buf->bufsize,
                                                (uint16_t)address);
            if (error_msg != 0) {
                ERR_PRINT("i2c_read failed with error %i\n", error_msg);
            }
        }
        else {
            int error_msg = zjs_i2c_handle_burst_read((uint8_t)bus,
                                                      buf->buffer,
                                                      buf->bufsize,
                                                      (uint16_t)address,
                                                      (uint16_t)register_addr);
            if (error_msg != 0) {
                ERR_PRINT("i2c_read failed with error %i\n", error_msg);
            }
        }
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

    // args: device, buffer[, register]
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_OBJECT, Z_OPTIONAL Z_NUMBER);

    uint32_t register_addr = 0;

    if (argc >= 3) {
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
        return zjs_error("zjs_i2c_write: missing data buffer");
    }

    uint32_t address = (uint32_t)jerry_get_number_value(argv[0]);

    int error_msg = zjs_i2c_handle_write((uint8_t)bus,
                                         dataBuf->buffer,
                                         dataBuf->bufsize,
                                         (uint16_t)address);

    return jerry_create_number(error_msg);
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

    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    jerry_value_t data = argv[0];
    uint32_t bus;
    uint32_t speed;

    if (!zjs_obj_get_uint32(data, "bus", &bus)) {
        return zjs_error("zjs_i2c_open: missing required field (bus)");
    }

    if (!zjs_obj_get_uint32(data, "speed", &speed)) {
        return zjs_error("zjs_i2c_open: missing required field (speed)");
    }

    if (zjs_i2c_handle_open((uint8_t)bus)) {
        return zjs_error("zjs_i2c_open: failed to open connection to I2C bus");
    }
    // create the I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    jerry_set_prototype(i2c_obj, zjs_i2c_prototype);

    zjs_obj_add_readonly_number(i2c_obj, bus, "bus");
    zjs_obj_add_readonly_number(i2c_obj, speed, "speed");

    return i2c_obj;
}

jerry_value_t zjs_i2c_init()
{
    #ifdef CONFIG_BOARD_ARDUINO_101
    zjs_i2c_ipm_init();
    #endif

    zjs_native_func_t array[] = {
        { zjs_i2c_read, "read" },
        { zjs_i2c_burst_read, "burstRead" },
        { zjs_i2c_write, "write" },
        { zjs_i2c_abort, "abort" },
        { zjs_i2c_close, "close" },
        { NULL, NULL }
    };
    zjs_i2c_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_i2c_prototype, array);

    // create global I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    zjs_obj_add_function(i2c_obj, zjs_i2c_open, "open");
    return i2c_obj;
}

void zjs_i2c_cleanup()
{
    jerry_release_value(zjs_i2c_prototype);
}

#endif // QEMU_BUILD
