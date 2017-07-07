// Copyright (c) 2017, Intel Corporation.

#ifndef __zjs_board_h__
#define __zjs_board_h__

#include "jerryscript.h"

// ZJS includes
#include "zjs_common.h"

struct device;

/**
 * Initialize the board module, or reinitialize after cleanup
 *
 * @return Board API object
 */
jerry_value_t zjs_board_init();

typedef struct zjs_pin {
    const char *name;      // string name
    const char *altname;   // alternate name, e.g. number as string
    const char *altname2;  // second alternate name, e.g. PWM channel
    u8_t device;
    u8_t gpio;
#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
    u8_t gpio_ss;
#endif
    u8_t pwm;
} zjs_pin_t;

// return codes from zjs_board_find_pin
#define FIND_PIN_FAILURE -1
#define FIND_PIN_INVALID -2

/**
 * Given a pin specifier from JS, find the appropriate pin info
 *
 * @param pin      A string or number from JS defining a pin
 * @param devname  Receives the port device name used to access this pin
 * @param pin_num  Receives the pin number to access the given pin
 *
 * @return 0 on success, FIND_PIN_FAILURE for a general failure, or
 *         FIND_PIN_INVALID if it was the wrong JS type
 */
int zjs_board_find_pin(jerry_value_t pin, char devname[20], int *pin_num);

#endif  // __zjs_board_h__
