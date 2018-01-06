// Copyright (c) 2017, Intel Corporation.

// NOTE: This is not really a normal header file but made to be included by
//   zjs_board.c when building for Arduino 101.

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
