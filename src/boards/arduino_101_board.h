// Copyright (c) 2017, Intel Corporation.

// NOTE: This is not really a normal header file but made to be included by
//   zjs_board.c when building for Arduino 101.

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
