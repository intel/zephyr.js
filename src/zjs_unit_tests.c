// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ZJS includes
#include "zjs_board.h"
#include "zjs_callbacks.h"
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

static int check_hex_to_byte(char *str, u8_t byte)
{
    u8_t result;
    zjs_hex_to_byte(str, &result);
    return result == byte;
}

static u32_t count1 = 0;
static void c_callback1(void *handle, const void *args)
{
    count1++;
}

static u8_t string_correct = 0;
static void c_callback2(void *handle, const void *args)
{
    char *s = (char *)handle;
    if (strequal(s, "pass string as handle")) {
        string_correct = 1;
    }
}

static u8_t handle_and_args = 0;
static void c_callback3(void *handle, const void *args)
{
    char *h = (char *)handle;
    char *a = (char *)args;
    if (strequal(h, "this is my handle")) {
        if (strequal(a, "this is my args")) {
            handle_and_args = 1;
        }
    }
}

static u8_t cb4_called = 0;
static void c_callback4(void *handle, const void *args)
{
    cb4_called = 1;
}

static void test_c_callbacks()
{
    zjs_init_callbacks();
    // test multiple signals
    zjs_callback_id id1 = zjs_add_c_callback(NULL, c_callback1);
    for (int i = 0; i < 10; i++) {
        zjs_signal_callback(id1, NULL, 0);
    }
    zjs_service_callbacks();
    zjs_assert(count1 == 10, "signal callback 10 times");
    zjs_remove_callback(id1);

    // test handle
    char s[] = "pass string as handle";
    zjs_callback_id id2 = zjs_add_c_callback(s, c_callback2);
    zjs_signal_callback(id2, NULL, 0);
    zjs_service_callbacks();
    zjs_assert(string_correct, "handle passed correctly");
    zjs_remove_callback(id2);

    // test args and handle
    char h[] = "this is my handle";
    char a[] = "this is my args";
    zjs_callback_id id3 = zjs_add_c_callback(h, c_callback3);
    zjs_signal_callback(id3, a, strlen(a));
    zjs_service_callbacks();
    zjs_assert(handle_and_args, "handle and args passed correctly");
    zjs_remove_callback(id3);

    // test callback ID recycling
    zjs_callback_id list[10];
    for (int i = 0; i < 10; i++) {
        list[i] = zjs_add_c_callback(NULL, c_callback4);
    }
    zjs_callback_id next = zjs_add_c_callback(NULL, c_callback4);
    for (int i = 0; i < 10; i++) {
        zjs_remove_callback(list[i]);
    }
    zjs_service_callbacks();
    zjs_callback_id less = zjs_add_c_callback(NULL, c_callback4);
    zjs_assert(less < next, "callback IDs are recycled");
    zjs_remove_callback(next);
    zjs_remove_callback(less);

    // test zjs_call_callback
    zjs_callback_id id4 = zjs_add_c_callback(NULL, c_callback4);
    zjs_call_callback(id4, NULL, 0);
    zjs_assert(cb4_called, "zjs_call_callback()");
    zjs_remove_callback(id4);
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

// Test zjs_util int compression functions

static int check_compress_reversible(u32_t num)
{
    // checks whether uncompressing compressed value returns exact match
    u32_t reversed = zjs_uncompress_16_to_32(zjs_compress_32_to_16(num));
    return num == reversed;
}

static int check_compress_close(u32_t num)
{
    // checks whether uncompressing compressed value is within 0.05% of original
    // 11-bit mantissa means 1/2048 ~= 0.05%
    u32_t reversed = zjs_uncompress_16_to_32(zjs_compress_32_to_16(num));
    double ratio = reversed * 1.0 / num;
    return ratio >= 0.9995 && ratio <= 1.0005;
}

static void test_compress_32()
{
    // expect these to be returned exactly
    zjs_assert(check_compress_reversible(0), "compression of 0");
    zjs_assert(check_compress_reversible(0x0cab), "compression of 0x0cab");
    zjs_assert(check_compress_reversible(0x7fff), "compression of 0x7fff");

    // these should be close
    zjs_assert(check_compress_close(0x8000), "compression of 0x8000");
    zjs_assert(check_compress_close(0xdeadbeef), "compression of 0xdeadbeef");
    zjs_assert(check_compress_close(0xffffffff), "compression of 0xffffffff");
}

// Test test_validate_args function

static ZJS_DECL_FUNC(dummy_func)
{
    return ZJS_UNDEFINED;
}

static void test_validate_args()
{
    // create some dummy objects
    ZVAL obj1 = zjs_create_object();
    ZVAL str1 = jerry_create_string((jerry_char_t *)"arthur");
    ZVAL func1 = jerry_create_external_function(dummy_func);
    ZVAL array1 = jerry_create_array(3);
    ZVAL num1 = jerry_create_number(42);
    ZVAL bool1 = jerry_create_boolean(true);
    ZVAL null1 = jerry_create_null();
    ZVAL undef1 = jerry_create_undefined();

    // create expect strings
    const char *expect_none[]  = { NULL };
    const char *expect_array[] = { Z_ARRAY, NULL };
    const char *expect_bool[]  = { Z_BOOL, NULL };
    const char *expect_func[]  = { Z_FUNCTION, NULL };
    const char *expect_null[]  = { Z_NULL, NULL };
    const char *expect_num[]   = { Z_NUMBER, NULL };
    const char *expect_obj[]   = { Z_OBJECT, NULL };
    const char *expect_str[]   = { Z_STRING, NULL };
    const char *expect_undef[] = { Z_UNDEFINED, NULL };

    // low expectations
    zjs_assert(zjs_validate_args(expect_none, 0, NULL) == 0,
               "none asked, none given");

    jerry_value_t argv[5];
    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_none, 1, argv) == 0,
               "expect none, pass the pi");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_none, 1, argv) == 0,
               "expect none, pass an object");

    // expect a string
    zjs_assert(zjs_validate_args(expect_str, 0, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect string, pass nothing");

    argv[0] = str1;
    zjs_assert(zjs_validate_args(expect_str, 1, argv) == 0,
               "expect string, pass string");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_str, 1, argv) == ZJS_INVALID_ARG,
               "expect string, pass object");

    argv[0] = str1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_str, 2, argv) == 0,
               "expect string, pass string and object");

    // expect an object
    zjs_assert(zjs_validate_args(expect_obj, 0, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect object, pass nothing");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass object");

    argv[0] = func1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass function");

    argv[0] = array1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass array");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == ZJS_INVALID_ARG,
               "expect object, pass number");

    // expect a function
    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_func, 1, argv) == ZJS_INVALID_ARG,
               "expect function, pass object");

    argv[0] = func1;
    zjs_assert(zjs_validate_args(expect_func, 1, argv) == 0,
               "expect function, pass function");

    // expect an array
    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_array, 1, argv) == ZJS_INVALID_ARG,
               "expect array, pass object");

    argv[0] = array1;
    zjs_assert(zjs_validate_args(expect_array, 1, argv) == 0,
               "expect array, pass array");

    // expect a number
    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_num, 1, argv) == 0,
               "expect number, pass number");

    argv[0] = null1;
    zjs_assert(zjs_validate_args(expect_num, 1, argv) == ZJS_INVALID_ARG,
               "expect number, pass null");

    // expect a bool
    argv[0] = bool1;
    zjs_assert(zjs_validate_args(expect_bool, 1, argv) == 0,
               "expect bool, pass bool");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_bool, 1, argv) == ZJS_INVALID_ARG,
               "expect bool, pass number");

    // expect a null
    argv[0] = null1;
    zjs_assert(zjs_validate_args(expect_null, 1, argv) == 0,
               "expect null, pass null");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == ZJS_INVALID_ARG,
               "expect null, pass number");

    // expect an undefined
    argv[0] = undef1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == 0,
               "expect undef, pass undef");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == ZJS_INVALID_ARG,
               "expect undef, pass number");

    // bogus expectation strings
    char too_low[2] = Z_ANY;
    too_low[0]--;
    const char *expect_low[] = { too_low, NULL };
    zjs_assert(zjs_validate_args(expect_low, 1, argv) == ZJS_INTERNAL_ERROR,
               "pass low expectation character");

    char too_high[2] = Z_UNDEFINED;
    too_high[0]++;
    const char *expect_high[] = { too_high, NULL };
    zjs_assert(zjs_validate_args(expect_high, 1, argv) == ZJS_INTERNAL_ERROR,
               "pass high expectation character");

    // optional argument
    const char *expect_opt[] = { Z_OPTIONAL Z_NUMBER, Z_OBJECT, NULL };
    argv[0] = num1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_opt, 2, argv) == 1,
               "expect optional number, required object, pass both");

    zjs_assert(zjs_validate_args(expect_opt, 1, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect optional number, required object, pass number");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_opt, 1, argv) == 0,
               "expect optional number, required object, pass object");

    // two optional arguments
    const char *expect_more[] = { Z_OPTIONAL Z_NUMBER, Z_OPTIONAL Z_NULL,
                                  Z_OBJECT, NULL };
    zjs_assert(zjs_validate_args(expect_more, 1, argv) == 0,
               "optional number and null, required object, pass object");

    argv[0] = num1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 2, argv) == 1,
               "optional number and null, required object, pass num + obj");

    argv[0] = null1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 2, argv) == 1,
               "optional number and null, required object, pass null + obj");

    argv[0] = num1;
    argv[1] = null1;
    argv[2] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 3, argv) == 2,
               "optional number and null, required object, pass all");
}

typedef struct test_list {
    int value;
    struct test_list *next;
} test_list_t;

static u8_t l1_freed = 0;
static u8_t l2_freed = 0;
static u8_t l3_freed = 0;

static void test_free_list(test_list_t *element)
{
    if (element->value == 1) {
        l1_freed = 1;
    } else if (element->value == 2) {
        l2_freed = 1;
    } else if (element->value == 3) {
        l3_freed = 1;
    }
}

static void test_list_macros()
{
    test_list_t *head = NULL;
    test_list_t l1e;
    l1e.value = 1;
    l1e.next = NULL;
    test_list_t l2e;
    l2e.value = 2;
    l2e.next = NULL;
    test_list_t l3e;
    l3e.value = 3;
    l3e.next = NULL;
    test_list_t *l1 = &l1e;
    test_list_t *l2 = &l2e;
    test_list_t *l3 = &l3e;

    // find node in a NULL list
    test_list_t *should_be_null = ZJS_LIST_FIND(test_list_t, NULL, value, 1);
    zjs_assert(should_be_null == NULL, "empty list returned NULL");

    // find node that doesn't exist
    should_be_null = ZJS_LIST_FIND(test_list_t, l2, value, 1);
    zjs_assert(should_be_null == NULL, "incorrect value not found");

    // find node that does exist
    test_list_t *t1 = ZJS_LIST_FIND(test_list_t, l1, value, 1);
    zjs_assert(t1 == l1, "correct node found in single element list");

    // create multi-element list
    ZJS_LIST_APPEND(test_list_t, head, l1);
    ZJS_LIST_APPEND(test_list_t, head, l2);
    ZJS_LIST_APPEND(test_list_t, head, l3);

    // check lengths
    zjs_assert(ZJS_LIST_LENGTH(test_list_t, head) == 3,
               "list length (l1) correct");
    zjs_assert(ZJS_LIST_LENGTH(test_list_t, head->next) == 2,
               "list length (l2) correct");
    zjs_assert(ZJS_LIST_LENGTH(test_list_t, head->next->next) == 1,
               "list length (l3) correct");

    // check values are correct
    test_list_t *tl1 = ZJS_LIST_FIND(test_list_t, head, value, 1);
    test_list_t *tl2 = ZJS_LIST_FIND(test_list_t, head, value, 2);
    test_list_t *tl3 = ZJS_LIST_FIND(test_list_t, head, value, 3);
    zjs_assert(tl1 == l1, "first element correct");
    zjs_assert(tl2 == l2, "second element correct");
    zjs_assert(tl3 == l3, "third element correct");

    // check order is correct
    zjs_assert(l1->next == l2, "l1.next == l2");
    zjs_assert(l2->next == l3, "l2.next == l3");

    // remove middle element
    if (ZJS_LIST_REMOVE(test_list_t, head, l2)) {
        zjs_assert(l1->next == l3, "middle element was removed");
        should_be_null = ZJS_LIST_FIND(test_list_t, head, value, 2);
        zjs_assert(should_be_null == NULL,
                   "middle element value was not found");
    } else {
        zjs_assert(0, "element was not removed");
    }

    // test prepend
    ZJS_LIST_PREPEND(test_list_t, head, l2);
    zjs_assert(head == l2, "prepended node");
    zjs_assert(ZJS_LIST_LENGTH(test_list_t, l2) == 3,
               "list length (prepend) correct");

    ZJS_LIST_FREE(test_list_t, head, test_free_list);
    zjs_assert(l1_freed, "l1 freed");
    zjs_assert(l2_freed, "l2 freed");
    zjs_assert(l3_freed, "l3 freed");

    zjs_assert(ZJS_LIST_LENGTH(test_list_t, head) == 0, "list length is zero");
    zjs_assert(head == NULL, "list head was NULL");
}

// Test zjs_str_matches function

static void test_str_matches()
{
    char *array[] = { "cow", "horse", "pig", NULL };
    zjs_assert(zjs_str_matches("cow", array), "string present");
    zjs_assert(!zjs_str_matches("dog", array), "string not present");
}

// Test split_pin_name in zjs_board.c

static void test_split_pin_name()
{
    char str[NAMED_PIN_MAX_LEN + 3];
    char prefix[NAMED_PIN_MAX_LEN];
    int number = 0;

    // set up str with len NAMED_PIN_MAX_LEN + 2
    for (int i = 0; i < NAMED_PIN_MAX_LEN + 2; i++) {
        str[i] = 'a';
    }
    str[NAMED_PIN_MAX_LEN + 2] = '\0';
    zjs_assert(wrap_split_pin_name(str, prefix, &number) == FIND_PIN_INVALID,
               "name at max len + 2");

    // terminate right at max len
    for (int i = 0; i < 2; i++) {
        str[NAMED_PIN_MAX_LEN + i] = 'b';
    }
    str[NAMED_PIN_MAX_LEN + 2] = '\0';
    zjs_assert(wrap_split_pin_name(str, prefix, &number) == FIND_PIN_INVALID,
               "name at max len");

    // terminate at one less than max
    str[NAMED_PIN_MAX_LEN - 1] = '\0';
    number = 0;
    zjs_assert(wrap_split_pin_name(str, prefix, &number) == 0,
               "name at max len - 1");
    zjs_assert(number == -1, "no number at max - 1");
    zjs_assert(strequal(str, prefix), "prefix is name at max - 1");

    // prefix plus number
    zjs_assert(wrap_split_pin_name("LED2", prefix, &number) == 0, "LED2 name");
    zjs_assert(strequal(prefix, "LED"), "LED2 prefix");
    zjs_assert(number == 2, "LED2 number");

    // just prefix
    zjs_assert(wrap_split_pin_name("BTN", prefix, &number) == 0, "BTN name");
    zjs_assert(strequal(prefix, "BTN"), "BTN prefix");
    zjs_assert(number == -1, "BTN number");

    // just number
    zjs_assert(wrap_split_pin_name("42", prefix, &number) == 0, "42 name");
    zjs_assert(strequal(prefix, ""), "42 prefix");
    zjs_assert(number == 42, "42 number");

    // extra junk after number
    zjs_assert(wrap_split_pin_name("A1A", prefix, &number) == FIND_PIN_INVALID,
               "junk after number");
}

void zjs_run_unit_tests()
{
    test_hex_to_byte();
    test_default_convert_pin();
    test_compress_32();
    test_validate_args();
    test_c_callbacks();
    test_list_macros();
    test_str_matches();
    test_split_pin_name();

    printf("TOTAL - %d of %d passed\n", passed, total);
    exit(!(passed == total));
}
