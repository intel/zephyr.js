// Copyright (c) 2016, Intel Corporation.

#include <string.h>

// ZJS includes
#include "zjs_util.h"

void zjs_set_property(const jerry_value_t obj, const char *str,
                      const jerry_value_t prop)
{
    jerry_value_t name = jerry_create_string((const jerry_char_t *)str);
    jerry_set_property(obj, name, prop);
    jerry_release_value(name);
}

jerry_value_t zjs_get_property(const jerry_value_t obj, const char *name)
{
    // requires: obj is an object, name is a property name string
    //  effects: looks up the property name in obj, and returns it; the value
    //             will be owned by the caller and must be released
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t rval = jerry_get_property(obj, jname);
    jerry_release_value(jname);
    return rval;
}

void zjs_obj_add_boolean(jerry_value_t obj, bool flag, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to value
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jbool = jerry_create_boolean(flag);
    jerry_set_property(obj, jname, jbool);
    jerry_release_value(jname);
    jerry_release_value(jbool);
}

void zjs_obj_add_function(jerry_value_t obj, void *func, const char *name)
{
    // requires: obj is an existing JS object, function is a native C function
    //  effects: creates a new field in object named name, that will be a JS
    //             JS function that calls the given C function

    // NOTE: The docs on this function make it look like func obj should be
    //   released before we return, but in a loop of 25k buffer creates there
    //   seemed to be no memory leak. Reconsider with future intelligence.
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jfunc = jerry_create_external_function(func);
    if (jerry_value_is_function(jfunc)) {
        jerry_set_property(obj, jname, jfunc);
    }
    jerry_release_value(jname);
    jerry_release_value(jfunc);
}

void zjs_obj_add_object(jerry_value_t parent, jerry_value_t child,
                        const char *name)
{
    // requires: parent and child are existing JS objects
    //  effects: creates a new field in parent named name, that refers to child
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_set_property(parent, jname, child);
    jerry_release_value(jname);
}

void zjs_obj_add_string(jerry_value_t obj, const char *str, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to sval
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jstr = jerry_create_string((const jerry_char_t *)str);
    jerry_set_property(obj, jname, jstr);
    jerry_release_value(jname);
    jerry_release_value(jstr);
}

void zjs_obj_add_number(jerry_value_t obj, double num, const char *name)
{
    // requires: obj is an existing JS object
    //  effects: creates a new field in parent named name, set to nval
    jerry_value_t jname = jerry_create_string((const jerry_char_t *)name);
    jerry_value_t jnum = jerry_create_number(num);
    jerry_set_property(obj, jname, jnum);
    jerry_release_value(jname);
    jerry_release_value(jnum);
}

bool zjs_obj_get_boolean(jerry_value_t obj, const char *name, bool *flag)
{
    // requires: obj is an existing JS object, value name should exist as
    //             boolean
    //  effects: retrieves field specified by name as a boolean
    jerry_value_t value = zjs_get_property(obj, name);
    if (jerry_value_has_error_flag(value))
        return false;

    if (!jerry_value_is_boolean(value))
        return false;

    *flag = jerry_get_boolean_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_string(jerry_value_t obj, const char *name, char *buffer,
                        int len)
{
    // requires: obj is an existing JS object, value name should exist as
    //             string, buffer can receive the string, len is its size
    //  effects: retrieves field specified by name; if it exists, and is a
    //             string, and can fit into the given buffer, copies it plus
    //             a null terminator into buffer and returns true; otherwise,
    //             returns false
    jerry_value_t value = zjs_get_property(obj, name);
    if (jerry_value_has_error_flag(value))
        return false;

    if (!jerry_value_is_string(value))
        return false;

    jerry_size_t jlen = jerry_get_string_size(value);
    if (jlen >= len)
        return false;

    int wlen = jerry_string_to_char_buffer(value, (jerry_char_t *)buffer, jlen);
    buffer[wlen] = '\0';

    jerry_release_value(value);

    return true;
}

bool zjs_obj_get_double(jerry_value_t obj, const char *name, double *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj, name);
    if (jerry_value_has_error_flag(value))
        return false;

    *num = jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_uint32(jerry_value_t obj, const char *name, uint32_t *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a uint32
    jerry_value_t value = zjs_get_property(obj, name);
    if (jerry_value_has_error_flag(value))
        return false;

    *num = (uint32_t)jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_obj_get_int32(jerry_value_t obj, const char *name, int32_t *num)
{
    // requires: obj is an existing JS object, value name should exist as number
    //  effects: retrieves field specified by name as a int32
    jerry_value_t value = zjs_get_property(obj, name);
    if (jerry_value_has_error_flag(value))
        return false;

    *num = (int32_t)jerry_get_number_value(value);
    jerry_release_value(value);
    return true;
}

bool zjs_hex_to_byte(char *buf, uint8_t *byte)
{
    // requires: buf is a string with at least two hex chars
    //  effects: converts the first two hex chars in buf to a number and
    //             returns it in *byte; returns true on success, false otherwise
    uint8_t num = 0;
    for (int i=0; i<2; i++) {
        num <<= 4;
        if (buf[i] >= 'A' && buf[i] <= 'F')
            num += buf[i] - 'A' + 10;
        else if (buf[i] >= 'a' && buf[i] <= 'f')
            num += buf[i] - 'a' + 10;
        else if (buf[i] >= '0' && buf[i] <= '9')
            num += buf[i] - '0';
        else return false;
    }
    *byte = num;
    return true;
}

void zjs_default_convert_pin(uint32_t orig, int *dev, int *pin) {
    // effects: reads top three bits of orig and writes them to dev and the
    //            bottom five bits and writes them to pin; thus up to eight
    //            devices are supported and up to 32 pins each; but if orig is
    //            0xff, returns 0 for dev and -1 for pin
    if (orig == 0xff) {
        *dev = 0;
        *pin = -1;
    }
    else {
        *dev = (orig & 0xe0) >> 5;
        *pin = orig & 0x1f;
    }
}

jerry_value_t zjs_error(const char *error)
{
    PRINT("%s\n", error);
    return jerry_create_error(JERRY_ERROR_TYPE, (jerry_char_t *)error);
}

#ifdef DEBUG_BUILD

static uint8_t init = 0;
static int seconds = 0;

#ifdef ZJS_LINUX_BUILD
#include <time.h>

int zjs_get_sec(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return now.tv_sec - seconds;
}

int zjs_get_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!init) {
        seconds = now.tv_sec;
        init = 1;
    }
    return (now.tv_nsec / 1000000);
}
#else

// Timer granularity
#define MS_PER_TICK    1000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC
static struct nano_timer print_timer;
// Millisecond counter to increment
static uint32_t milli = 0;
// Dummy user handle so nano_task_timer_test() works
static void* dummy = (void*)0xFFFFFFFF;

void update_print_timer(void)
{
    if (!init) {
        nano_timer_init(&print_timer, dummy);
        nano_timer_start(&print_timer, MS_PER_TICK);
        init = 1;
    }
    if (nano_task_timer_test(&print_timer, TICKS_NONE)) {
        if (milli >= 100) {
            milli = 0;
            seconds++;
        } else {
            milli += MS_PER_TICK;
        }
        nano_timer_start(&print_timer, MS_PER_TICK);
    }
}

int zjs_get_sec(void)
{
    if (!init) {
        nano_timer_init(&print_timer, dummy);
        nano_timer_start(&print_timer, MS_PER_TICK);
        init = 1;
    }
    return seconds;
}

int zjs_get_ms(void)
{
    if (!init) {
        nano_timer_init(&print_timer, dummy);
        nano_timer_start(&print_timer, MS_PER_TICK);
        init = 1;
    }
    return milli;
}
#endif // ZJS_LINUX_BUILD
#endif // DEBUG_BUILD
