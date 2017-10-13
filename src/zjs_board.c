// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_common.h"
#include "zjs_util.h"

#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
// data from zephyr/boards/x86/arduino_101/pinmux.c
// *dual pins mean there's another zephyr pin that maps to the same user pin,
//    exposing distinct functionality; unclear how to understand this so far
static const zjs_pin_t pin_data[] = {
    {"IO0",  "0",  NULL,   0, 255,   9, 255},
    {"IO1",  "1",  NULL,   0, 255,   8, 255},
    {"IO2",  "2",  NULL,   0,  18, 255, 255},
    {"IO3",  "3",  "PWM0", 0,  17,  10,   0},
    {"IO4",  "4",  NULL,   0,  19, 255, 255},
    {"IO5",  "5",  "PWM1", 0,  15,  11,   1},
    {"IO6",  "6",  "PWM2", 0, 255,  12,   2},
    {"IO7",  "7",  NULL,   0,  20, 255, 255},
    {"IO8",  "8",  NULL,   0,  16, 255, 255},
    {"IO9",  "9",  "PWM3", 0, 255,  13,   3},
    {"IO10", "10", NULL,   0,  11,   0, 255},
    {"IO11", "11", NULL,   0,  10,   3, 255},
    {"IO12", "12", NULL,   0,   9,   1, 255},
    {"IO13", "13", "LED2", 0,   8,   2, 255},
    {"AD0",  "A0", NULL,   0, 255,  10, 255},
    {"AD1",  "A1", NULL,   0, 255,  11, 255},
    {"AD2",  "A2", NULL,   0, 255,  12, 255},
    {"AD3",  "A3", NULL,   0, 255,  13, 255},
    {"AD4",  "A4", NULL,   0, 255,  14, 255},
    {"AD5",  "A5", NULL,   0, 255,   9, 255},
    {"LED0", NULL, NULL,   0,  26, 255, 255},
    {"LED1", NULL, NULL,   0,  12, 255, 255}
};
#elif CONFIG_BOARD_FRDM_K64F
enum gpio_ports {
    PTA, PTB, PTC, PTD, PTE
};

static const zjs_pin_t pin_data[] = {
    {"D0",    "0",  NULL, PTC, 16, 255},
    {"D1",    "1",  NULL, PTC, 17, 255},
    {"D2",    "2",  NULL, PTB,  9, 255},
    {"D3",    "3",  NULL, PTA,  1, 255},
    {"D4",    "4",  NULL, PTB, 23, 255},
    {"D5",    "5",  NULL, PTA,  2, 255},
    {"D6",    "6",  NULL, PTC,  2, 255},
    {"D7",    "7",  NULL, PTC,  3, 255},
    // currently not working on rev E3; used to work as input/output
    {"D8",    "8",  NULL, PTC, 12, 255},  // PTA0 for Rev <= D
    {"D9",    "9",  NULL, PTC,  4, 255},
    {"D10",  "10",  NULL, PTD,  0, 255},
    {"D11",  "11",  NULL, PTD,  2, 255},
    {"D12",  "12",  NULL, PTD,  3, 255},
    {"D13",  "13",  NULL, PTD,  1, 255},
    {"D14",  "14",  NULL, PTE, 25, 255},
    {"D15",  "15",  NULL, PTE, 24, 255},
    {"A0",   NULL,  NULL, PTB,  2, 255},
    {"A1",   NULL,  NULL, PTB,  3, 255},
    {"A2",   NULL,  NULL, PTB, 10, 255},
    {"A3",   NULL,  NULL, PTB, 11, 255},
    {"A4",   NULL,  NULL, PTC, 11, 255},
    {"A5",   NULL,  NULL, PTC, 10, 255},
    {"LEDR", NULL,  NULL, PTB, 22, 255},
    {"LEDG", NULL,  NULL, PTE, 26, 255},
    {"LEDB", NULL,  NULL, PTB, 21, 255},
    {"SW2",  NULL,  NULL, PTC,  6, 255},
    {"SW3",  NULL,  NULL, PTA,  4, 255},
    {"PWM0", NULL,  NULL, PTA,  1,   0},
    {"PWM1", NULL,  NULL, PTA,  2,   2},
    {"PWM2", NULL,  NULL, PTC,  2,   3},
    {"PWM3", NULL,  NULL, PTC,  3,   3},
    {"PWM4", NULL,  NULL, PTC, 12,   4},
    {"PWM5", NULL,  NULL, PTC,  4,   5},
    {"PWM6", NULL,  NULL, PTD,  0,   6},
    {"PWM7", NULL,  NULL, PTD,  2,   7},
    {"PWM8", NULL,  NULL, PTD,  3,   8},
    {"PWM9", NULL,  NULL, PTD,  1,   9}
    // TODO: More pins at https://developer.mbed.org/platforms/FRDM-K64F/
};
#else
// for boards without pin name support, use pass-through method
static const zjs_pin_t pin_data[] = {};
#endif

static const zjs_pin_t *zjs_find_pin(const char *name)
{
    // effects: searches all name fields in pin_data array for a match
    //            with name, and returns the matching record or NULL
    int len = sizeof(pin_data) / sizeof(zjs_pin_t);
    for (int i = 0; i < len; ++i) {
        const zjs_pin_t *pin = &pin_data[i];
        if (strequal(name, pin->name) ||
            (pin->altname && strequal(name, pin->altname)) ||
            (pin->altname2 && strequal(name, pin->altname2)))
            return pin;
    }
    return NULL;
}

int zjs_board_find_pin(jerry_value_t pin, char devname[20], int *pin_num)
{
    // requires: pin should be a number or string, else returns FIND_PIN_INVALID
    const int LEN = 20;

    ZVAL_MUTABLE pin_str = 0;
    if (jerry_value_is_number(pin)) {
        // no error is possible w/ number or string
        pin_str = jerry_value_to_string(pin);
        pin = pin_str;
    }

    if (jerry_value_is_string(pin)) {
        jerry_size_t size = 32;
        char name[size];
        zjs_copy_jstring(pin, name, &size);
        DBG_PRINT("Looking for pin '%s'\n", name);
        if (!size) {
            return FIND_PIN_FAILURE;
        }

        const zjs_pin_t *pin_desc = zjs_find_pin(name);
        if (pin_desc) {
            int bytes = snprintf(devname, LEN, "GPIO_%d", pin_desc->device);
            if (bytes >= LEN || pin_desc->gpio == 255) {
                return FIND_PIN_FAILURE;
            }
            *pin_num = pin_desc->gpio;
            return 0;
        }

        // no pin found: try generic method

        // Pin ID can be a string of format "GPIODEV.num", where
        // GPIODEV is Zephyr's native device name for GPIO port,
        // this is usually GPIO_0, GPIO_1, etc., but some Zephyr
        // ports have completely different naming, so don't assume
        // anything! "num" is a numeric pin number within the port,
        // usually within range 0-31.
        char *dot = strchr(name, '.');
        if (!dot || dot == name || dot[1] == '\0') {
            return FIND_PIN_FAILURE;
        }
        *dot = '\0';
        char *end;
        int ipin = (int)strtol(dot + 1, &end, 10);
        if (*end != '\0')
            return FIND_PIN_FAILURE;
        int bytes = snprintf(devname, LEN, "%s", name);
        if (bytes >= LEN || ipin < 0) {
            return FIND_PIN_FAILURE;
        }
        *pin_num = ipin;
        return 0;
    }

    // pin input was invalid
    return FIND_PIN_INVALID;
}

#ifdef CONFIG_BOARD_ARDUINO_101
#define BOARD_NAME "arduino_101"
#elif ZJS_LINUX_BUILD
#define BOARD_NAME "linux (partially simulating arduino_101)"
#elif CONFIG_BOARD_FRDM_K64F
#define BOARD_NAME "frdm_k64f"
#elif CONFIG_BOARD_NRF52_PCA10040
#define BOARD_NAME "nrf52_pca10040"
#elif CONFIG_BOARD_ARDUINO_DUE
#define BOARD_NAME "arduino_due"
#elif CONFIG_BOARD_NUCLEO_F411RE
#define BOARD_NAME "nucleo_f411re"
#else
#define BOARD_NAME "unknown"
#endif

static jerry_value_t zjs_board_init()
{
    // create board object
    jerry_value_t board_obj = zjs_create_object();
    zjs_obj_add_readonly_string(board_obj, "name", BOARD_NAME);
    zjs_obj_add_readonly_string(board_obj, "version", "0.1");

    return board_obj;
}

JERRYX_NATIVE_MODULE (board, zjs_board_init)
