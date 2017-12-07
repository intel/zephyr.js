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

// E.g. to map IO3-5 to pin entries 7-9, use:
//   prefix "IO", offset 3, start 7, end 9
typedef struct {
    const char *prefix;
    pin_id_t offset;
    pin_id_t start;
    pin_id_t end;
} prefix_range_t;

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

#ifdef BUILD_MODULE_GPIO
// prefix before GPIO device id
static const char *digital_prefix = "GPIO_";

// map gpio_ports integers to device id characters
static const char *digital_convert = "0";

// data from zephyr/boards/x86/arduino_101/pinmux.c
static const pin_map_t digital_pins[] = {
    {0, 255},  // IO0
    {0, 255},  // IO1
    {0,  18},  // IO2
    {0,  17},  // IO3
    {0,  19},  // IO4
    {0,  15},  // IO5
    {0, 255},  // IO6
    {0,  20},  // IO7
    {0,  16},  // IO8
    {0, 255},  // IO9
    {0,  11},  // IO10
    {0,  10},  // IO11
    {0,   9},  // IO12
    {0,   8},  // IO13
    {0,  26},  // LED0
    {0,  12},  // LED1
    {0,   8},  // LED2 (aka IO13)
};

static const prefix_range_t digital_map[] = {
    {"IO",  0,  0, 13},  // IO0 - IO13
    {"LED", 0, 14, 16}   // LED0 - LED2
};

static const extra_pin_t digital_extras[] = {};
#endif  // BUILD_MODULE_GPIO

#ifdef BUILD_MODULE_AIO
static const char *analog_prefix = "ADC_";
static const char *analog_convert = "0";
static const pin_map_t analog_pins[] = {
    {0, 10},  // AD0 (pin numbers are for ARC side where these are used)
    {0, 11},  // AD1
    {0, 12},  // AD2
    {0, 13},  // AD3
    {0, 14},  // AD4
    {0,  9},  // AD5
};
static const prefix_range_t analog_map[] = {
    {"AD", 0, 0, 5},  // AD0 - AD5
    {"A", 0, 0, 5}    // A0 - A5
};
static const extra_pin_t analog_extras[] = {};
#endif  // BUILD_MODULE_AIO

#ifdef BUILD_MODULE_PWM
static const char *pwm_prefix = "PWM_";
static const char *pwm_convert = "0";
static const pin_map_t pwm_pins[] = {
    {0, 0},  // PWM0 = IO3
    {0, 1},  // PWM1 = IO5
    {0, 2},  // PWM2 = IO6
    {0, 3}   // PWM3 = IO9
};
static const prefix_range_t pwm_map[] = {
    {"PWM", 0, 0, 3}
};
static const extra_pin_t pwm_extras[] = {
    {"IO3", 0},
    {"IO5", 1},
    {"IO6", 2},
    {"IO9", 3}
};
#endif  // BUILD_MODULE_PWM

#elif CONFIG_BOARD_FRDM_K64F
//
// FRDM-K64F board support
//

#ifdef BUILD_MODULE_GPIO
enum gpio_ports {
    PTA, PTB, PTC, PTD, PTE
};

// prefix before GPIO device id
static const char *digital_prefix = "GPIO_";

// map gpio_ports integers to device id characters
static const char *digital_convert = "01234";

static const pin_map_t digital_pins[] = {
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
    {PTB, 22},  // LED0 (LEDR)
    {PTE, 26},  // LED1 (LEDG)
    {PTB, 21},  // LED2 (LEDB)
    {PTC,  6},  // SW2
    {PTA,  4},  // SW3
};

static const prefix_range_t digital_map[] = {
    {"D",   0,  0, 15},  // D0 - D15
    {"LED", 0, 16, 18},  // LED0 - LED2
    {"SW",  2, 19, 20}   // SW2 - SW3
};

// special named pins that aren't assigned by integer mapping
static const extra_pin_t digital_extras[] = {
    {"LEDR", 16},
    {"LEDG", 17},
    {"LEDB", 18},
};
#endif  // BUILD_MODULE_GPIO

#ifdef BUILD_MODULE_AIO
enum aio_ports {
    ADC0, ADC1
};

// prefix before GPIO device id
static const char *analog_prefix = "ADC_";

// map gpio_ports integers to device id characters
static const char *analog_convert = "01";

// Documentation of pin mappings on the K64F board:
//   https://os.mbed.com/platforms/FRDM-K64F/
// Page 2 of schematic correlates pin names to ADC controller/channel:
//   https://www.nxp.com/downloads/en/schematics/FRDM-K64F-SCH-C.pdf
static const pin_map_t analog_pins[] = {
    {ADC0, 12},  // A0 = PTB2
    {ADC0, 13},  // A1 = PTB3
    {ADC1, 14},  // A2 = PTB10
    {ADC1, 15},  // A3 = PTB11
                 // A4 = PTC11
                 // A5 = PTC10
    {ADC1, 18},  // A6 (ZJS shortcut) is labeled ADC1_SE18
    {ADC0, 23},  // A7 (ZJS shortcut) is labeled DAC0_OUT
};
// A4 and A5 are hardware triggered (denoted by the B in ADC1_SE7B), not
//   supported by Zephyr currently
// The SoC has other analog inputs but they don't seem to be wired up
//   on the K64F board; also some others are differential inputs that
//   Zephyr doesn't support currently (e.g. ADC0_DM0/DP0)

static const prefix_range_t analog_map[] = {
    {"A", 0, 0, 3},  // A0 - A3
    {"A", 6, 4, 5}   // A6 - A7
};

static const extra_pin_t analog_extras[] = {};
#endif  // BUILD_MODULE_AIO

#ifdef BUILD_MODULE_PWM
enum pwm_ports {
    FTM0, FTM1, FTM2, FTM3
};

// prefix before PWM device id
static const char *pwm_prefix = "PWM_";

// map pwm_ports integers to device id characters
static const char *pwm_convert = "0123";

static const pin_map_t pwm_pins[] = {
    // names PWM0-11 are ZJS-only shortcuts not found in K64F docs
    {FTM0, 6},  // PWM0  = PTA1  = D3
    {FTM0, 7},  // PWM1  = PTA2  = D5
    {FTM0, 1},  // PWM2  = PTC2  = D6
    {FTM0, 2},  // PWM3  = PTC3  = D7
                // PWM4  = PTC12 = D8 (not sure what FTM3_FLT0 means)
    {FTM0, 3},  // PWM5  = PTC4  = D9
    {FTM3, 0},  // PWM6  = PTD0  = D10
    {FTM3, 2},  // PWM7  = PTD2  = D11
    {FTM3, 3},  // PWM8  = PTD3  = D12
    {FTM3, 1},  // PWM9  = PTD1  = D13
    {FTM3, 7},  // PWM10 = PTC11 = A4
    {FTM3, 6},  // PWM11 = PTC10 = A5
};

static const prefix_range_t pwm_map[] = {
    {"PWM", 0, 0,  3},  // PWM0 - PWM3
    {"PWM", 5, 4, 10},  // PWM5 - PWM11
    {"D",   3, 0,  0},  // D3
    {"D",   5, 1,  3},  // D5 - D7
    {"D",   9, 4,  8}   // D9 - D13
    {"A",   4, 9, 10}   // A4 - A5
};

static const extra_pin_t pwm_extras[] = {};
#endif  // BUILD_MODULE_PWM

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

static int split_pin_name(const char *name, char *prefix, int *number)
{
    // requires: name is a pin name from JavaScript; prefix is a buffer with
    //             at least NAMED_PIN_MAX_LEN bytes; number is a pointer to
    //             an int to receive numeric portion of pin name
    //  effects: if name is NAMED_PIN_MAX_LEN or longer, returns a
    //             FIND_PIN_INVALID; if name contains characters following
    //             digits, returns FIND_PIN_INVALID; otherwise, if there are
    //             no digits, copies name to prefix and sets number to -1; if
    //             there are digits, copies any alphabet prefix in name to
    //             prefix, then parses the numeric value into number; on
    //             success, returns 0
    ZJS_ASSERT(name, "name is NULL");

    if (strnlen(name, NAMED_PIN_MAX_LEN) == NAMED_PIN_MAX_LEN) {
        DBG_PRINT("pin name too long: '%s'\n", name);
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

    long num = -1;
    if (index != -1) {
        // at least one digit was found
        char *end;
        num = strtol(name + index, &end, 10);
        if (*end) {
            DBG_PRINT("invalid pin suffix: '%s'\n", end);
            return FIND_PIN_INVALID;
        }
    }

    strncpy(prefix, name, index);
    prefix[index] = '\0';
    *number = num;
    return 0;
}

static int find_pin_id(const char *name,
                       const prefix_range_t map[], int map_len,
                       const extra_pin_t extra[], int extra_len)
{
    // requires: name is a pin name from JavaScript; map is an array of
    //             prefixes with range and offset that map to pins; map_len is
    //             the number of entries in map; extra is an array of special
    //             pin numbers and pin ids; extra_len is the number of entries
    //             in extra
    //  effects: searches for a pin name matching name, which may have an alpha
    //             prefix and may have a numeric suffix, but at least one of the
    //             two; if the name is numeric only, uses the first prefix in
    //             map to validate the pin, otherwise one based on the prefix;
    //             returns the id (row) in map that matches, or <0 on error

    char prefix[NAMED_PIN_MAX_LEN];
    int number;
    int rval = split_pin_name(name, prefix, &number);
    if (rval < 0) {
        return rval;
    }

    int id = -1;
    if (number != -1 && map_len > 0) {
        // at least one digit was found
        if (prefix[0] == '\0') {
            // no prefix found; use default from first entry
            strcpy(prefix, map[0].prefix);
        }

        // prefix found, find match to determine range
        for (int i = 0; i < map_len; ++i) {
            if (strequal(prefix, map[i].prefix)) {
                // if number in range, use it
                int num = number - map[i].offset + map[i].start;
                if (num <= map[i].end) {
                    id = num;
                    break;
                }
            }
        }
    }

    if (id < 0) {
        // still not found, try extras
        for (int i = 0; i < extra_len; ++i) {
            if (strequal(name, extra[i].name)) {
                id = extra[i].id;
            }
        }
    }

    if (id < 0) {
        DBG_PRINT("pin not found: '%s'\n", name);
        return FIND_PIN_FAILURE;
    }

    return id;
}

// do not call this directly, use the macro below
static int find_named_pin_priv(const char *name,
                               const pin_map_t *pins, size_t pins_size,
                               const prefix_range_t *map, size_t map_size,
                               const extra_pin_t *extras, int extras_size,
                               const char *prefix, const char *convert,
                               char *device_name, int name_len)
{
    // requires: name is a pin name from JavaScript; pins is an array mapping
    //             pin id to device id / number; pins_size is the size in bytes
    //             of the array; extras is an array of other friendly pin names
    //             and the id in pins they correspond to; extras_size is the
    //             size in bytes of the array; prefix is the prefix to the
    //             device name, e.g. GPIO_ for GPIO_0 device; convert is a
    //             string mapping device ids to the character that follows the
    //             prefix for a specific device driver; device_name is a place
    //             to store the device driver name, name_len is the receiving
    //             buffer length
    //  effects: searches for a pin name matching name, which may have an alpha
    //             prefix and may have a numeric suffix, but at least one of the
    //             two; if the name is numeric only, uses the first prefix that
    //             occurs in map to validate the pin, otherwise one based on the
    //             prefix; when the pin is found, returns the right device name
    //             in device-name, and returns the correct Zephyr pin
    int map_len = map_size / sizeof(prefix_range_t);
    int extra_len = extras_size / sizeof(extra_pin_t);
    int id = find_pin_id(name, map, map_len, extras, extra_len);
    if (id < 0) {
        return id;
    }

    ZJS_ASSERT(id < pins_size / sizeof(pin_map_t), "pin id overflow");

    if (pins[id].zpin == 255) {
        // mode not supported
        DBG_PRINT("unsupported mode for id %d\n", id);
        return FIND_PIN_FAILURE;
    }

    // pin found, return results
    int written = snprintf(device_name, name_len, "%s%d", prefix,
                           pins[id].device);
    if (written >= name_len) {
        DBG_PRINT("couldn't find device '%s'\n", name);
        return FIND_DEVICE_FAILURE;
    }

    DBG_PRINT("returning a pin %d, %d\n", id, pins[id].zpin);
    return pins[id].zpin;
}

// call this instead of the above function, see above for arg descriptions
#define find_named_pin(name, pins, map, extras, prefix, convert,    \
                       device_name, name_len)                       \
    find_named_pin_priv(name, pins, sizeof(pins), map, sizeof(map), \
                        extras, sizeof(extras), prefix, convert,    \
                        device_name, name_len)

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

    DBG_PRINT("pin '%s' is not a full pin\n", pin_name);
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
        return find_named_pin(name, digital_pins, digital_map, digital_extras,
                              digital_prefix, digital_convert,
                              device_name, len);
    }

#endif
    DBG_PRINT("gpio device name: '%s'\n", device_name);
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
        pin = find_named_pin(name, analog_pins, analog_map, analog_extras,
                             analog_prefix, analog_convert, device_name, len);
    }
#endif
    DBG_PRINT("Returning pin %s:%d\n", device_name, pin);
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
        pin = find_named_pin(name, pwm_pins, pwm_map, pwm_extras,
                             pwm_prefix, pwm_convert, device_name, len);
    }
#endif
    DBG_PRINT("pwm device name: '%s'\n", device_name);
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
