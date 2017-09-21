// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_common.h"
#include "zjs_gpio.h"
#include "zjs_util.h"

#define PIN_NAME_MAX_LEN 8

typedef u8_t pin_id_t;

typedef struct {
    pin_id_t start;
    pin_id_t end;
} pin_range_t;

typedef struct {
    const char *name;
    pin_id_t id;
} extra_pin_t;

#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
// data from zephyr/boards/x86/arduino_101/pinmux.c
// *dual pins mean there's another zephyr pin that maps to the same user pin,
//    exposing distinct functionality; unclear how to understand this so far
static const zjs_pin_t pin_data[] = {
    {0, 255,   9},  // IO0
    {0, 255,   8},  // IO1
    {0,  18, 255},  // IO2
    {0,  17,  10},  // IO3
    {0,  19, 255},  // IO4
    {0,  15,  11},  // IO5
    {0, 255,  12},  // IO6
    {0,  20, 255},  // IO7
    {0,  16, 255},  // IO8
    {0, 255,  13},  // IO9
    {0,  11,   0},  // IO10
    {0,  10,   3},  // IO11
    {0,   9,   1},  // IO12
    {0,   8,   2},  // IO13
    {0, 255,  10},  // AD0
    {0, 255,  11},  // AD1
    {0, 255,  12},  // AD2
    {0, 255,  13},  // AD3
    {0, 255,  14},  // AD4
    {0, 255,   9},  // AD5
    {0,  26, 255},  // LED0
    {0,  12, 255},  // LED1
    {0,   8,   2},  // LED2 (aka IO13)
    {0,  17,  10},  // PWM0 (aka IO3)
    {0,  15,  11},  // PWM1 (aka IO5)
    {0, 255,  12},  // PWM2 (aka IO6)
    {0, 255,  13}   // PWM3 (aka IO9)
};

static const pin_range_t digital_pins = {0, 13};
static const pin_range_t analog_pins = {14, 19};
static const pin_range_t led_pins = {20, 22};
static const pin_range_t pwm_pins = {23, 26};
static const extra_pin_t extra_pins[] = {};

#elif CONFIG_BOARD_FRDM_K64F
enum gpio_ports {
    PTA, PTB, PTC, PTD, PTE
};

static const zjs_pin_t pin_data[] = {
    {PTC, 16},  // D0
    {PTC, 17},  // D1
    {PTB,  9},  // D2
    {PTA,  1},  // D3
    {PTB, 23},  // D4
    {PTA,  2},  // D5
    {PTC,  2},  // D6
    {PTC,  3},  // D7
    // D8 currently not working on rev E3; used to work as input/output
    {PTC, 12},  // D8 (PTA0 for Rev <= D)
    {PTC,  4},  // D9
    {PTD,  0},  // D10
    {PTD,  2},  // D11
    {PTD,  3},  // D12
    {PTD,  1},  // D13
    {PTE, 25},  // D14
    {PTE, 24},  // D15
    {PTB,  2},  // A0
    {PTB,  3},  // A1
    {PTB, 10},  // A2
    {PTB, 11},  // A3
    {PTC, 11},  // A4
    {PTC, 10},  // A5
    {PTB, 22},  // LEDR
    {PTE, 26},  // LEDG
    {PTB, 21},  // LEDB
    {PTC,  6},  // SW2
    {PTA,  4},  // SW3
    {PTA,  1},  // PWM0
    {PTA,  2},  // PWM1
    {PTC,  2},  // PWM2
    {PTC,  3},  // PWM3
    {PTC, 12},  // PWM4
    {PTC,  4},  // PWM5
    {PTD,  0},  // PWM6
    {PTD,  2},  // PWM7
    {PTD,  3},  // PWM8
    {PTD,  1}   // PWM9
    // TODO: More pins at https://developer.mbed.org/platforms/FRDM-K64F/
};

static const pin_range_t digital_pins = {0, 15};
static const pin_range_t analog_pins = {16, 21};
static const pin_range_t led_pins = {22, 24};
static const pin_range_t pwm_pins = {27, 36};
static const extra_pin_t extra_pins[] = {
    {"LEDR", 22},
    {"LEDG", 23},
    {"LEGB", 24},
    { "SW2", 25},
    { "SW3", 26}
};

#else
// for boards without pin name support, use pass-through method
static const zjs_pin_t pin_data[] = {};
#endif

typedef struct {
    const char *prefix;
    const pin_range_t *range;
} prefix_t;

static const prefix_t prefix_map[] = {
    { "IO", &digital_pins},
    {  "D", &digital_pins},
    { "AD", &analog_pins},
    {  "A", &analog_pins},
    {"LED", &led_pins},
    {"PWM", &pwm_pins}
};

const zjs_pin_t *zjs_find_pin(const char *name)
{
    // effects: searches all name fields in pin_data array for a match
    //            with name, and returns the matching record or NULL
    if (strnlen(name, PIN_NAME_MAX_LEN) == PIN_NAME_MAX_LEN) {
        // pin name too long
        return NULL;
    }

    // find where number starts
    int index = -1;
    for (int i = 0; name[i]; ++i) {
        if (name[i] >= '0' && name[i] <= '9') {
            index = i;
            break;
        }
    }

    pin_id_t id = 0;

    if (index != -1) {
        const pin_range_t *range = NULL;
        if (!index) {
            range = &digital_pins;
        }
        else {
            char prefix[index + 1];
            strncpy(prefix, name, index);
            prefix[index] = '\0';
            int len = sizeof(prefix_map) / sizeof(prefix_t);
            for (int i = 0; i < len; ++i) {
                if (strequal(prefix, prefix_map[i].prefix)) {
                    range = prefix_map[i].range;
                }
            }
        }

        if (range) {
            int num = atoi(name + index) + range->start;
            if (num < range->start || num > range->end) {
                // apparently out of range, but check "extras"
                index = -1;
            }
            id = num;
        }
    }

    if (index == -1) {
        // still not found, try extras
        int len = sizeof(extra_pins) / sizeof(extra_pin_t);
        for (int i = 0; i < len; ++i) {
            if (strequal(name, extra_pins[i].name)) {
                index = 0;
                id = extra_pins[i].id;
                break;
            }
        }

        if (index == -1) {
            return NULL;
        }
    }

    ZJS_ASSERT(id < sizeof(pin_data) / sizeof(zjs_pin_t), "pin id overflow");
    return pin_data + id;
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
        if (*end != '\0') {
            return FIND_PIN_FAILURE;
        }
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

jerry_value_t zjs_board_init()
{
    // create board object
    jerry_value_t board_obj = zjs_create_object();
    zjs_obj_add_readonly_string(board_obj, "name", BOARD_NAME);
    zjs_obj_add_readonly_string(board_obj, "version", "0.1");

    return board_obj;
}
