// Copyright (c) 2016-2018, Intel Corporation.

#ifndef QEMU_BUILD
// C includes
#include <string.h>

// Zephyr includes
#include <i2c.h>

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_i2c_handler.h"
#include "zjs_util.h"

#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_i2c_ipm_handler.h"
#include "zjs_ipm.h"
#endif

static jerry_value_t zjs_i2c_prototype;

static ZJS_DECL_FUNC_ARGS(zjs_i2c_read_base, bool burst)
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

    u32_t register_addr = 0;
    u32_t size = (u32_t)jerry_get_number_value(argv[1]);

    if (argc >= 3) {
        register_addr = (u32_t)jerry_get_number_value(argv[2]);
    }

    if (size < 1) {
        return zjs_error("size should be greater than zero");
    }

    u32_t bus;
    zjs_obj_get_uint32(this, "bus", &bus);
    u32_t address = (u32_t)jerry_get_number_value(argv[0]);
    zjs_buffer_t *buf;
    jerry_value_t buf_obj = zjs_buffer_create(size, &buf);
    if (buf) {
        // If this is a burst read or a register address is given
        if (burst || register_addr != 0) {
            int error_msg =
                zjs_i2c_handle_burst_read((u8_t)bus, buf->buffer, buf->bufsize,
                                          (u16_t)address, (u16_t)register_addr);
            if (error_msg != 0) {
                ERR_PRINT("i2c_read failed with error %i\n", error_msg);
            }
        } else {
            int error_msg = zjs_i2c_handle_read((u8_t)bus, buf->buffer,
                                                buf->bufsize, (u16_t)address);
            if (error_msg != 0) {
                ERR_PRINT("i2c_read failed with error %i\n", error_msg);
            }
        }
    }

    return buf_obj;
}

static ZJS_DECL_FUNC(zjs_i2c_read)
{
    // requires: Requires two arguments and has an optional third.
    //           arg[0] - Address of the I2C device you wish to read from.
    //           arg[1] - Number of bytes to read
    //           arg[2] - Register address you wish to read from on the
    //                    device. (Optional. Default address is 0x00)
    //  effects: Reads the number of bytes requested from the I2C device
    //           from the register address. Returns a buffer object
    //           that size containing the data.

    return zjs_i2c_read_base(function_obj, this, argv, argc, false);
}

static ZJS_DECL_FUNC(zjs_i2c_burst_read)
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

    return zjs_i2c_read_base(function_obj, this, argv, argc, true);
}

static ZJS_DECL_FUNC(zjs_i2c_write)
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
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_BUFFER, Z_OPTIONAL Z_NUMBER);

    u32_t register_addr = 0;

    if (argc >= 3) {
        register_addr = (u32_t)jerry_get_number_value(argv[2]);
    }

    u32_t bus;
    zjs_obj_get_uint32(this, "bus", &bus);
    zjs_buffer_t *dataBuf = zjs_buffer_find(argv[1]);

    if (register_addr != 0) {
        // If the user supplied a register address, add it to the beginning
        // of the buffer, as that's where i2c_write will expect it.
        dataBuf->buffer[0] = (u8_t)register_addr;
    }

    u32_t address = (u32_t)jerry_get_number_value(argv[0]);

    int error_msg = zjs_i2c_handle_write((u8_t)bus, dataBuf->buffer,
                                         dataBuf->bufsize, (u16_t)address);

    return jerry_create_number(error_msg);
}

static ZJS_DECL_FUNC(zjs_i2c_abort)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_i2c_close)
{
    // Not implemented yet
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_i2c_open)
{
    // requires: Requires two arguments
    //           arg[0] - I2C bus number you want to open a connection to.
    //           arg[1] - Bus speed in kbps.
    //  effects: Creates a I2C object connected to the bus number specified.

    // args: initialization object
    ZJS_VALIDATE_ARGS(Z_OBJECT);

    jerry_value_t data = argv[0];
    u32_t bus;
    u32_t speed;

    if (!zjs_obj_get_uint32(data, "bus", &bus)) {
        return zjs_error("missing required field (bus)");
    }

    if (!zjs_obj_get_uint32(data, "speed", &speed)) {
        return zjs_error("missing required field (speed)");
    }

    if (zjs_i2c_handle_open((u8_t)bus)) {
        return zjs_error("failed to open connection to I2C bus");
    }
    // create the I2C object
    jerry_value_t i2c_obj = zjs_create_object();
    jerry_set_prototype(i2c_obj, zjs_i2c_prototype);

    zjs_obj_add_readonly_number(i2c_obj, "bus", bus);
    zjs_obj_add_readonly_number(i2c_obj, "speed", speed);

    return i2c_obj;
}

static void zjs_i2c_cleanup(void *native)
{
    jerry_release_value(zjs_i2c_prototype);
}

static const jerry_object_native_info_t i2c_module_type_info = {
    .free_cb = zjs_i2c_cleanup
};

static jerry_value_t zjs_i2c_init()
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
    zjs_i2c_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_i2c_prototype, array);

    // create global I2C object
    jerry_value_t i2c_obj = zjs_create_object();
    zjs_obj_add_function(i2c_obj, "open", zjs_i2c_open);
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(i2c_obj, NULL, &i2c_module_type_info);
    return i2c_obj;
}

JERRYX_NATIVE_MODULE(i2c, zjs_i2c_init)
#endif  // QEMU_BUILD
