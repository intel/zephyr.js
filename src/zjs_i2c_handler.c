// Copyright (c) 2016-2017, Intel Corporation.
#include "zjs_i2c_handler.h"
#include "zjs_common.h"

#define I2C_0 0
#define I2C_1 1
#define MAX_I2C_BUS 2

static struct device *i2c_device[MAX_I2C_BUS];

uint8_t zjs_i2c_handle_open(uint8_t msg_bus) {
    uint8_t error_code = 0;
    if (msg_bus < MAX_I2C_BUS) {
        char bus[6];
        snprintf(bus, 6, "I2C_%i", msg_bus);
        i2c_device[msg_bus] = device_get_binding(bus);

        if (!i2c_device[msg_bus]) {
                ZJS_PRINT("I2C bus %s not found.\n", bus);
                error_code = 1;
        } else {
            /* TODO remove these hard coded numbers
             * once the config API is made */
            union dev_config cfg;
            cfg.raw = 0;
            cfg.bits.use_10_bit_addr = 0;
            cfg.bits.speed = I2C_SPEED_STANDARD;
            cfg.bits.is_master_device = 1;

            if (i2c_configure(i2c_device[msg_bus], cfg.raw) != 0) {
                ZJS_PRINT("I2C bus %s configure failed.\n", bus);
                error_code = 1;
            }
        }
    } else {
        ZJS_PRINT("I2C bus I2C_%i is not a valid I2C bus.\n", msg_bus);
        error_code = 1;
    }
    return error_code;
}

uint8_t zjs_i2c_handle_write(uint8_t msg_bus,
                             uint8_t *data,
                             uint32_t length,
                             uint16_t address) {
    uint8_t error_code = 0;
    if (msg_bus < MAX_I2C_BUS) {
        // Write has to come after an Open I2C message
        if (i2c_device[msg_bus]) {
            error_code = i2c_write(i2c_device[msg_bus],
                                   data,
                                   length,
                                   address);

                if (error_code != 0)
                    ZJS_PRINT("i2c_write failed!\n");
        }
        else {
            ZJS_PRINT("no I2C device is ready yet\n");
            error_code = 1;
        }
    }
    return error_code;
}

uint8_t zjs_i2c_handle_read(uint8_t msg_bus,
                            uint8_t *data,
                            uint32_t length,
                            uint16_t address) {
    uint8_t error_code = 0;
    if (msg_bus < MAX_I2C_BUS) {
        // Read has to come after an Open I2C message
        if (i2c_device[msg_bus]) {

            int reply = i2c_read(i2c_device[msg_bus],
                                 data,
                                 length,
                                 address);

            if (reply < 0) {
                error_code = 1;
            }
        }
        else {
            ZJS_PRINT("No I2C device is ready yet\n");
            error_code = 1;
        }
    }
    return error_code;
}

uint8_t zjs_i2c_handle_burst_read(uint8_t msg_bus,
                                  uint8_t *data,
                                  uint32_t length,
                                  uint16_t address,
                                  uint16_t register_addr) {
    uint8_t error_code = 0;
    if (msg_bus < MAX_I2C_BUS) {
        // Burst read has to come after an Open I2C message
        if (i2c_device[msg_bus]) {
           int reply = i2c_burst_read(i2c_device[msg_bus],
                                      address,
                                      register_addr,
                                      data,
                                      length);

            if (reply < 0) {
                error_code = 1;
            }
        }
        else {
            ZJS_PRINT("No I2C device is ready yet\n");
            error_code = 1;
        }
    }
    return error_code;
}
