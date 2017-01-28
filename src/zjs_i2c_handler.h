// Copyright (c) 2016-2017, Intel Corporation.
#ifndef __zjs_i2c_handler_h__
#define __zjs_i2c_handler_h__

#include <i2c.h>

int zjs_i2c_handle_open(uint8_t msg_bus);
int zjs_i2c_handle_write(uint8_t msg_bus, uint8_t *data, uint32_t length, uint16_t address);
int zjs_i2c_handle_read(uint8_t msg_bus, uint8_t *data, uint32_t length, uint16_t address);
int zjs_i2c_handle_burst_read(uint8_t msg_bus, uint8_t *data, uint32_t length, uint16_t address, uint16_t register_addr);

#endif  // __zjs_i2c_handler_h__
