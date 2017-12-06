// Copyright (c) 2017, Intel Corporation.

// C includes
#include <string.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_common.h"
#include "zjs_util.h"

// max length of a named pin like IO3, PWM0, LED0
#define NAMED_PIN_MAX_LEN 8

// max length of a pass-through pin like GPIO_0.12
#define FULL_PIN_MAX_LEN 16

typedef u8_t pin_id_t;

typedef struct {
    u8_t device;
    u8_t zpin;
#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
    u8_t zpin_ss;
#endif
} pin_map_t;

typedef struct {
    pin_id_t start;
    pin_id_t end;
} pin_range_t;

typedef struct {
    const char *name;
    pin_id_t id;
} extra_pin_t;

typedef struct {
    pin_id_t from;
    pin_id_t to;
} pin_remap_t;

#if defined(BUILD_MODULE_GPIO) || defined(BUILD_MODULE_PWM) || defined(BUILD_MODULE_AIO)

#if defined(CONFIG_BOARD_ARDUINO_101) || defined(ZJS_LINUX_BUILD)
//
// Arduino 101 board support
//

// data from zephyr/boards/x86/arduino_101/pinmux.c
// *dual pins mean there's another zephyr pin that maps to the same user pin,
//    exposing distinct functionality; unclear how to understand this so far
static const pin_map_t pin_data[] = {
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
    {0,  26, 255},  // LED0
    {0,  12, 255},  // LED1
    {0,   8,   2},  // LED2 (aka IO13)
    {0, 255,  10},  // AD0
    {0, 255,  11},  // AD1
    {0, 255,  12},  // AD2
    {0, 255,  13},  // AD3
    {0, 255,  14},  // AD4
    {0, 255,   9},  // AD5
    {0,   0, 255},  // PWM0 (aka IO3)
    {0,   1, 255},  // PWM1 (aka IO5)
    {0,   2, 255},  // PWM2 (aka IO6)
    {0,   3, 255}   // PWM3 (aka IO9)
};

static const pin_range_t digital_pins = {0, 13};
#ifdef BUILD_MODULE_GPIO
static const pin_range_t led_pins = {14, 16};
#endif
#ifdef BUILD_MODULE_AIO
static const pin_range_t analog_pins = {17, 22};
#endif
static const extra_pin_t extra_pins[] = {};

#ifdef BUILD_MODULE_PWM
static const pin_range_t pwm_pins = {23, 26};
static const pin_remap_t pwm_map[] = {  // maps normal pins to PWM pins
    {3, 23},
    {5, 24},
    {6, 25},
    {9, 26}
};
#endif

#elif CONFIG_BOARD_FRDM_K64F
//
// FRDM-K64F board support
//

enum gpio_ports {
    PTA, PTB, PTC, PTD, PTE
};

static const pin_map_t pin_data[] = {
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
    {PTB, 22},  // LEDR
    {PTE, 26},  // LEDG
    {PTB, 21},  // LEDB
    {PTC,  6},  // SW2
    {PTA,  4},  // SW3
    {PTB,  2},  // A0
    {PTB,  3},  // A1
    {PTB, 10},  // A2
    {PTB, 11},  // A3
    {PTC, 11},  // A4
    {PTC, 10},  // A5
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
#ifdef BUILD_MODULE_GPIO
static const pin_range_t led_pins = {16, 18};
#endif
#ifdef BUILD_MODULE_AIO
static const pin_range_t analog_pins = {21, 26};
#endif
static const extra_pin_t extra_pins[] = {
#ifdef BUILD_MODULE_GPIO
    {"LEDR", 16},
    {"LEDG", 17},
    {"LEDB", 18},
    { "SW2", 19},
    { "SW3", 20}
#endif
};

#ifdef BUILD_MODULE_PWM
static const pin_range_t pwm_pins = {27, 36};
static const pin_remap_t pwm_map[] = {
    { 3, 27},
    { 5, 28},
    { 6, 29},
    { 7, 30},
    { 8, 31},
    { 9, 32},
    {10, 33},
    {11, 34},
    {12, 35},
    {13, 36}
};
#endif

#else
//
// Basic support for all other boards
//

// for boards without pin name support, use pass-through method
#define BASIC_BOARD_SUPPORT
#endif  // end of board support

typedef struct {
    const char *prefix;
    const pin_range_t *range;
} prefix_t;

#ifndef BASIC_BOARD_SUPPORT
static const prefix_t prefix_map[] = {
    { "IO", &digital_pins},
    {  "D", &digital_pins},
#ifdef BUILD_MODULE_GPIO
    {"LED", &led_pins},
#endif
#ifdef BUILD_MODULE_AIO
    { "AD", &analog_pins},
    {  "A", &analog_pins},
#endif
#ifdef BUILD_MODULE_PWM
    {"PWM", &pwm_pins}
#endif
};

static int find_named_pin(const char *name, const pin_range_t *default_range,
                          char *device_prefix, const pin_remap_t *remap,
                          int remap_len, char *device_name, int name_len)
{
    // requires: name is a pin name from JavaScript; default_range specifies the
    //             start and end values if name is a numeric pin; device_prefix
    //             is the prefix string e.g. GPIO for GPIO_0 device; remap is
    //             an array of pin numbers that should be remapped if found, or
    //             NULL; remap_len is the length of that array, and device_name
    //             is a place to store the device driver name, name_len is the
    //             receiving buffer length
    //  effects: searches for a pin name matching name, which may have an alpha
    //             prefix and may have a numeric suffix, but at least one of the
    //             two; if the name is numeric only, uses default_range to
    //             validate the pin, otherwise one based on the prefix; when the
    //             pin is found, remaps it to another pin if remap is specified;
    //             finally returns the right device name in device-name, and
    //             returns the correct Zephyr pin
    ZJS_ASSERT(name, "name is NULL");
    ZJS_ASSERT(default_range, "default_range is NULL");
    ZJS_ASSERT(device_prefix, "device_prefix is NULL");

    if (strnlen(name, NAMED_PIN_MAX_LEN) == NAMED_PIN_MAX_LEN) {
        DBG_PRINT("pin name too long\n");
        return FIND_PIN_INVALID;
    }

    // find where number starts
    int index = -1;
    for (int i = 0; name[i]; ++i) {
        if (name[i] >= '0' && name[i] <= '9') {
            index = i;
            break;
        }
    }

    int id = -1;

    if (index != -1) {
        // at least one digit was found
        const pin_range_t *range = NULL;
        if (!index) {
            // no prefix found; treat raw number as a default range
            range = default_range;
        }
        else {
            // prefix found, find match to determine range
            char prefix[index + 1];
            strncpy(prefix, name, index);
            prefix[index] = '\0';
            int len = sizeof(prefix_map) / sizeof(prefix_t);
            for (int i = 0; i < len; ++i) {
                if (strequal(prefix, prefix_map[i].prefix)) {
                    range = prefix_map[i].range;
                    break;
                }
            }
        }

        if (range) {
            char *end;
            long num = strtol(name + index, &end, 10) + range->start;
            if (*end) {
                return FIND_PIN_INVALID;
            }

            if (num >= range->start && num <= range->end) {
                id = num;
            }
        }
    }

    if (id < 0) {
        // still not found, try extras
        int len = sizeof(extra_pins) / sizeof(extra_pin_t);
        for (int i = 0; i < len; ++i) {
            if (strequal(name, extra_pins[i].name)) {
                id = extra_pins[i].id;
            }
        }
    }

    if (id < 0) {
        return FIND_PIN_FAILURE;
    }

    // remap the pin if needed
    for (int i = 0; i < remap_len; ++i) {
        if (remap[i].from == id) {
            id = remap[i].to;
            break;
        }
    }

    ZJS_ASSERT(id < sizeof(pin_data) / sizeof(pin_map_t), "pin id overflow");

    if (pin_data[id].zpin == 255) {
        // mode not supported
        DBG_PRINT("unsupported mode for id %d\n", id);
        return FIND_PIN_FAILURE;
    }

    // pin found, return results
    int written = snprintf(device_name, name_len, "%s_%d", device_prefix,
                           pin_data[id].device);
    if (written >= name_len) {
        DBG_PRINT("couldn't find device '%s'\n", name);
        return FIND_DEVICE_FAILURE;
    }

    return pin_data[id].zpin;
}
#endif  // !BASIC_BOARD_SUPPORT

static int find_full_pin(const char *name, const char *pin,
                         char *device_name, int name_len)
{
    // requires: name is the name portion and pin the pin portion of a
    //             name.pin pass-through pin spec (e.g. GPIO_0.17)
    //  effects: on failure, returns a negative value; on success writes the
    //             specified device name to device_name returns unvalidated
    //             pin number
    char *end = NULL;
    long num = strtol(pin, &end, 10);
    if (*end) {
        return FIND_PIN_INVALID;
    }

    int written = snprintf(device_name, name_len, "%s", name);
    if (written >= name_len) {
        DBG_PRINT("couldn't find device '%s'\n", name);
        return FIND_DEVICE_FAILURE;
    }

    return num;
}

static jerry_value_t pin2string(jerry_value_t jspin, int *rval)
{
    // effects: sets rval to 0 on success, <0 otherwise
    *rval = 0;
    if (jerry_value_is_number(jspin)) {
        // no error is possible w/ number or string
        return jerry_value_to_string(jspin);
    }

    if (jerry_value_is_string(jspin)) {
        return jerry_acquire_value(jspin);
    }

    DBG_PRINT("pin value wrong type\n");
    *rval = FIND_PIN_INVALID;
    return ZJS_UNDEFINED;
}

static int find_pin(jerry_value_t jspin, char *pin_name,
                    char *device_name, int name_len)
{
    // requires: pin_name is an output buffer with at least FULL_PIN_MAX_LEN
    //             bytes, device_name is a buffer to receive driver name, and
    //             name_len is the length of that buffer
    //  effects: tries to resolve pin as a full pin name, like GPIO_0.15
    //           returns pin number if found; returns FIND_PIN_INVALID on
    //           error; returns FIND_PIN_FAILURE if it's not a full pin
    //           but rather a named pin and writes pin name to pin_name
    int rval;
    ZVAL pin = pin2string(jspin, &rval);
    if (rval < 0) {
        return rval;
    }

    jerry_size_t size = FULL_PIN_MAX_LEN;
    zjs_copy_jstring(pin, pin_name, &size);
    if (!size) {
        DBG_PRINT("pin name too long\n");
        return FIND_PIN_INVALID;
    }

    char *ptr = strchr(pin_name, '.');
    if (ptr) {
        // split the string into device and pin portions
        *ptr = '\0';
        return find_full_pin(pin_name, ptr + 1, device_name, name_len);
    }

    return FIND_PIN_FAILURE;
}
#endif  // BUILD_MODULE_GPIO || BUILD_MODULE_PWM || BUILD_MODULE_AIO

#ifdef BUILD_MODULE_GPIO
int zjs_board_find_gpio(jerry_value_t jspin, char *device_name, int len)
{
    char name[FULL_PIN_MAX_LEN];
    int pin = find_pin(jspin, name, device_name, len);

#ifndef BASIC_BOARD_SUPPORT
    if (pin == FIND_PIN_FAILURE) {
        pin = find_named_pin(name, &digital_pins, "GPIO", NULL, 0,
                             device_name, len);
    }
#endif
    return pin;
}
#endif

#ifdef BUILD_MODULE_AIO
int zjs_board_find_aio(jerry_value_t jspin, char *device_name, int len)
{
    char name[FULL_PIN_MAX_LEN];
    int pin = find_pin(jspin, name, device_name, len);

#ifndef BASIC_BOARD_SUPPORT
    if (pin == FIND_PIN_FAILURE) {
        pin = find_named_pin(name, &analog_pins, "AIO", NULL, 0,
                             device_name, len);
    }
#endif
    return pin;
}
#endif

#ifdef BUILD_MODULE_PWM
int zjs_board_find_pwm(jerry_value_t jspin, char *device_name, int len)
{
    char name[FULL_PIN_MAX_LEN];
    int pin = find_pin(jspin, name, device_name, len);

#ifndef BASIC_BOARD_SUPPORT
    if (pin == FIND_PIN_FAILURE) {
        int remap_len = sizeof(pwm_map) / sizeof(pin_remap_t);
        pin = find_named_pin(name, &pwm_pins, "PWM", pwm_map, remap_len,
                             device_name, len);
    }
#endif
    return pin;
}
#endif

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

JERRYX_NATIVE_MODULE(board, zjs_board_init)
