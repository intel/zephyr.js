// Copyright (c) 2016-2018, Intel Corporation.

// ZJS includes
#include "zjs_i2c_handler.h"
#include "zjs_common.h"

// The Arduino 101 has two I2C busses, but I2C_SS_1 isn't connected to anything.
// So we have disabled the ability to connect to it, to avoid user confusion
#ifdef CONFIG_BOARD_ARDUINO_101_SSS
#define MAX_I2C_BUS 1
#else
#define MAX_I2C_BUS 2
#endif

#define I2C_BUS "I2C_"

static struct device *i2c_device[MAX_I2C_BUS];

int zjs_i2c_handle_open(u8_t msg_bus)
{
    int error_code = -1;
    if (msg_bus < MAX_I2C_BUS) {
        char bus[9];
        snprintf(bus, 9, "%s%i", I2C_BUS, msg_bus);
        i2c_device[msg_bus] = device_get_binding(bus);

        if (!i2c_device[msg_bus]) {
            ERR_PRINT("I2C bus %s not found.\n", bus);
        } else {
            u32_t cfg;
            cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER;
            error_code = i2c_configure(i2c_device[msg_bus], cfg);

            if (error_code != 0) {
                ERR_PRINT("I2C bus %s configure failed with %i.\n", bus,
                          error_code);
            }
        }
    } else {
        ERR_PRINT("I2C bus %s%i is not a valid I2C bus.\n", I2C_BUS, msg_bus);
    }
    return error_code;
}

int zjs_i2c_handle_write(u8_t msg_bus, u8_t *data, u32_t length, u16_t address)
{
    int error_code = -1;
    if (msg_bus < MAX_I2C_BUS) {
        // Write has to come after an Open I2C message
        if (i2c_device[msg_bus]) {
            error_code = i2c_write(i2c_device[msg_bus], data, length, address);

            if (error_code != 0) {
                ERR_PRINT("i2c_write failed with %i\n", error_code);
            }
        } else {
            ERR_PRINT("no I2C device is ready yet\n");
        }
    } else {
        ERR_PRINT("I2C bus %s%i is not a valid I2C bus.\n", I2C_BUS, msg_bus);
    }
    return error_code;
}

int zjs_i2c_handle_read(u8_t msg_bus, u8_t *data, u32_t length, u16_t address)
{
    int error_code = -1;
    if (msg_bus < MAX_I2C_BUS) {
        // Read has to come after an Open I2C message
        if (i2c_device[msg_bus]) {

            error_code = i2c_read(i2c_device[msg_bus], data, length, address);
        } else {
            ERR_PRINT("No I2C device is ready yet\n");
        }
    } else {
        ERR_PRINT("I2C bus %s%i is not a valid I2C bus.\n", I2C_BUS, msg_bus);
    }
    return error_code;
}

int zjs_i2c_handle_burst_read(u8_t msg_bus, u8_t *data, u32_t length,
                              u16_t address, u16_t register_addr)
{
    int error_code = -1;
    if (msg_bus < MAX_I2C_BUS) {
        // Burst read has to come after an Open I2C message
        if (i2c_device[msg_bus]) {
            error_code = i2c_burst_read(i2c_device[msg_bus], address,
                                        register_addr, data, length);
        } else {
            ERR_PRINT("No I2C device is ready yet\n");
        }
    } else {
        ERR_PRINT("I2C bus %s%i is not a valid I2C bus.\n", I2C_BUS, msg_bus);
    }
    return error_code;
}
