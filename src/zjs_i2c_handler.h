// Copyright (c) 2016-2017, Intel Corporation.
#ifndef __zjs_i2c_handler_h__
#define __zjs_i2c_handler_h__

// Zephyr includes
#include <i2c.h>

int zjs_i2c_handle_open(u8_t msg_bus);

int zjs_i2c_handle_write(u8_t msg_bus, u8_t *data, u32_t length, u16_t address);

int zjs_i2c_handle_read(u8_t msg_bus, u8_t *data, u32_t length, u16_t address);

int zjs_i2c_handle_burst_read(u8_t msg_bus, u8_t *data, u32_t length,
                              u16_t address, u16_t register_addr);

#endif  // __zjs_i2c_handler_h__
