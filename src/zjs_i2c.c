// Copyright (c) 2016, Intel Corporation.
#ifndef QEMU_BUILD

// Zephyr includes
#include <i2c.h>
#include <string.h>

// ZJS includes
#include "zjs_i2c.h"
#include "zjs_util.h"
#include "zjs_buffer.h"

static struct k_sem i2c_sem;
static jerry_value_t zjs_i2c_prototype;

static struct device *i2c_device[MAX_I2C_BUS];

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

    // FIXME: This section has duplicated code that could be simplified; also
    //   bus >= MAX_I2C_BUS appears to be an error that should be caught?
    //   Also, MAX_I2C_BUS (=1) is treated as an invalid bus number; is that
    //   right? Because I2C_1 is set to 1 in zjs_i2c.h
    if (!burst) {
        if (bus < MAX_I2C_BUS) {
            // read has to come after an Open I2C message
            if (i2c_device[bus]) {
                int reply = i2c_read(i2c_device[bus], buf->buffer, size,
                                     (uint16_t)address);

                if (reply < 0) {
                    ERR_PRINT("I2C Read failed with error: %i\n", reply);
                }
            }
            else {
                ERR_PRINT("No I2C device is ready yet\n");
            }
        }

    } else {
        if (bus < MAX_I2C_BUS) {
            // burst read has to come after an Open I2C message
            if (i2c_device[bus]) {
               int reply = i2c_burst_read(i2c_device[bus], (uint16_t)address,
                                          register_addr, buf->buffer, size);

                if (reply < 0) {
                    ERR_PRINT("I2C Burst Read failed with error: %i\n", reply);
                }
            }
            else {
                ERR_PRINT("No I2C device is ready yet\n");
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

    if (argc < 2 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_i2c_write: missing arguments");
    }

    if (!jerry_value_is_object(argv[1])) {
        return zjs_error("zjs_i2c_write: message buffer is invalid");
    }

    uint32_t register_addr = 0;

    if (argc >= 3) {
        if (!jerry_value_is_number(argv[2])) {
            return zjs_error("zjs_i2c_read: register is not a number");
        } else {
            register_addr = (uint32_t)jerry_get_number_value(argv[2]);
        }
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
    int reply;
    uint32_t address = (uint32_t)jerry_get_number_value(argv[0]);

    if (bus < MAX_I2C_BUS && i2c_device[bus]) {
        // Write has to come after an Open I2C message
        reply = i2c_write(i2c_device[bus],
                              dataBuf->buffer,
                              dataBuf->bufsize,
                              (uint16_t)address);

        if (reply < 0) {
            ERR_PRINT("I2C Write failed with error: %i\n", reply);
        }
    }
    else {
        ERR_PRINT("No I2C device is ready yet\n");
    }

    return jerry_create_number(reply);

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

    if (bus < MAX_I2C_BUS) {
        char i2c_bus[6];
        snprintf(i2c_bus, 6, "I2C_%i", (uint8_t)bus);
        i2c_device[bus] = device_get_binding(i2c_bus);

        /* TODO remove these hard coded numbers
         * once the config API is made */
        union dev_config cfg;
        cfg.raw = 0;
        cfg.bits.use_10_bit_addr = 0;
        cfg.bits.speed = I2C_SPEED_STANDARD;
        cfg.bits.is_master_device = 1;

        int reply = i2c_configure(i2c_device[bus], cfg.raw);

        if (reply < 0) {
            ERR_PRINT("I2C bus %s configure failed with error: %i\n", i2c_bus,
                      reply);
        }

    } else {
        ERR_PRINT("I2C bus %i is not a valid I2C bus\n", (uint8_t)bus);
    }

    // create the I2C object
    jerry_value_t i2c_obj = jerry_create_object();
    jerry_set_prototype(i2c_obj, zjs_i2c_prototype);

    zjs_obj_add_number(i2c_obj, bus, "bus");
    zjs_obj_add_number(i2c_obj, speed, "speed");

    return i2c_obj;
}

jerry_value_t zjs_i2c_init()
{
    k_sem_init(&i2c_sem, 0, 1);

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
