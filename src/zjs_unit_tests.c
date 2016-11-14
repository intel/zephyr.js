// Copyright (c) 2016, Intel Corporation.

#include <stdio.h>
#include <stdlib.h>

#include "zjs_util.h"

static int passed = 0;
static int total = 0;

static void zjs_assert(int test, char *str)
{
    char *label = "\033[1m\033[31mFAIL\033[0m";
    if (test) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    total += 1;
    printf("%s - %s\n", label, str);
}

// Test zjs_hex_to_byte function

static int check_hex_to_byte(char *str, uint8_t byte)
{
    uint8_t result;
    zjs_hex_to_byte(str, &result);
    return result == byte;
}

static void test_hex_to_byte()
{
    zjs_assert(check_hex_to_byte("00", 0), "hex to byte: 00");
    zjs_assert(check_hex_to_byte("ff", 0xff), "hex to byte: ff");
    zjs_assert(check_hex_to_byte("FF", 0xff), "hex to byte: FF");
    zjs_assert(check_hex_to_byte("3a", 0x3a), "hex to byte: 3a");
    zjs_assert(check_hex_to_byte("b4", 0xb4), "hex to byte: b4");
    zjs_assert(check_hex_to_byte("Aa", 0xaa), "hex to byte: Aa");
}

// Test zjs_default_convert_pin function

static void test_default_convert_pin()
{
    int dev, pin;
    zjs_default_convert_pin(0x00, &dev, &pin);
    zjs_assert(dev == 0 && pin == 0, "convert pin 00 -> 0/0");
    zjs_default_convert_pin(0x27, &dev, &pin);
    zjs_assert(dev == 1 && pin == 7, "convert pin 27 -> 1/7");
    zjs_default_convert_pin(0xef, &dev, &pin);
    zjs_assert(dev == 7 && pin == 15, "convert pin fe -> 7/15");
    zjs_default_convert_pin(0xfe, &dev, &pin);
    zjs_assert(dev == 7 && pin == 30, "convert pin fe -> 7/31");
    zjs_default_convert_pin(0xff, &dev, &pin);
    zjs_assert(dev == 0 && pin == -1, "convert pin failure case");
}

void zjs_run_unit_tests()
{
    test_hex_to_byte();
    test_default_convert_pin();

    printf("TOTAL - %d of %d passed\n", passed, total);
    exit(!(passed == total));
}
