// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_GROVE_LCD
#ifndef QEMU_BUILD
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif
#include <device.h>
#include <string.h>
#include <display/grove_lcd.h>
#include <misc/util.h>

// ZJS includes
#include "zjs_grove_lcd.h"
#include "zjs_util.h"

#define MAX_BUFFER_SIZE 256

static struct device *glcd = NULL;

static jerry_value_t zjs_glcd_prototype;

static jerry_value_t zjs_glcd_print(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    // args: text
    ZJS_VALIDATE_ARGS(Z_STRING);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    jerry_size_t size = MAX_BUFFER_SIZE;
    char *buffer = zjs_alloc_from_jstring(argv[0], &size);
    if (!buffer) {
        return zjs_error("zjs_glcd_print: cannot allocate buffer");
    }

    glcd_print(glcd, buffer, size);
    DBG_PRINT("Grove LCD print: %s\n", buffer);
    zjs_free(buffer);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_clear(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    glcd_clear(glcd);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_cursor_pos(const jerry_value_t function_obj,
                                             const jerry_value_t this,
                                             const jerry_value_t argv[],
                                             const jerry_length_t argc)
{
    // args: column, row
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t col = (uint8_t)jerry_get_number_value(argv[0]);
    uint8_t row = (uint8_t)jerry_get_number_value(argv[1]);
    glcd_cursor_pos_set(glcd, col, row);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_select_color(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    // args: predefined color index
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(argv[0]);
    glcd_color_select(glcd, value);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_color(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    // args: red, green, blue
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t r = (uint8_t)jerry_get_number_value(argv[0]);
    uint8_t g = (uint8_t)jerry_get_number_value(argv[1]);
    uint8_t b = (uint8_t)jerry_get_number_value(argv[2]);
    glcd_color_set(glcd, r, g, b);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_function(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    // args: predefined function number
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(argv[0]);
    glcd_function_set(glcd, value);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_function(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = glcd_function_get(glcd);

    return jerry_create_number(value);
}

static jerry_value_t zjs_glcd_set_display_state(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(argv[0]);
    glcd_display_state_set(glcd, value);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_display_state(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = glcd_display_state_get(glcd);

    return jerry_create_number(value);
}

/*  This is not supported in Zephyr driver yet
static jerry_value_t zjs_glcd_set_input_state(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    // args: predefined numeric constants
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    uint8_t value = (uint8_t)jerry_get_number_value(argv[0]);
    glcd_input_state_set(glcd, value);

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_input_state(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    if (!glcd) {
        return zjs_error("Grove LCD not initialized");
    }

    uint8_t value = glcd_input_state_get(glcd);

    return jerry_create_number(value);
}
*/

static jerry_value_t zjs_glcd_init(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    if (!glcd) {
        /* Initialize the Grove LCD */
        glcd = device_get_binding(GROVE_LCD_NAME);

        if (!glcd) {
            return zjs_error("failed to initialize Grove LCD");
        } else {
            DBG_PRINT("Grove LCD initialized\n");
        }
    }

    // create the Grove LCD device object
    jerry_value_t dev_obj = jerry_create_object();
    jerry_set_prototype(dev_obj, zjs_glcd_prototype);
    return dev_obj;
}

jerry_value_t zjs_grove_lcd_init()
{
    zjs_native_func_t array[] = {
        { zjs_glcd_print, "print" },
        { zjs_glcd_clear, "clear" },
        { zjs_glcd_set_cursor_pos, "setCursorPos" },
        { zjs_glcd_select_color, "selectColor" },
        { zjs_glcd_set_color, "setColor" },
        { zjs_glcd_set_function, "setFunction" },
        { zjs_glcd_get_function, "getFunction" },
        { zjs_glcd_set_display_state, "setDisplayState" },
        { zjs_glcd_get_display_state, "getDisplayState" },
/*  This is not supported in Zephyr driver yet
        { zjs_glcd_set_input_state, "setInputState" },
        { zjs_glcd_get_input_state, "getInputState" },
*/
        { NULL, NULL }
    };
    zjs_glcd_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_glcd_prototype, array);

    // create global grove_lcd object
    jerry_value_t glcd_obj = jerry_create_object();
    zjs_obj_add_function(glcd_obj, zjs_glcd_init, "init");

    // function flags
    ZVAL_MUTABLE val = jerry_create_number(GLCD_FS_8BIT_MODE);
    zjs_set_property(glcd_obj, "GLCD_FS_8BIT_MODE", val);

    val = jerry_create_number(GLCD_FS_ROWS_2);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_2", val);

    val = jerry_create_number(GLCD_FS_ROWS_1);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_1", val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_BIG);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_BIG", val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_LITTLE);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_LITTLE", val);

    // display state flags
    val = jerry_create_number(GLCD_DS_DISPLAY_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_ON", val);

    val = jerry_create_number(GLCD_DS_DISPLAY_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_OFF", val);

    val = jerry_create_number(GLCD_DS_CURSOR_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_ON", val);

    val = jerry_create_number(GLCD_DS_CURSOR_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_OFF", val);

    val = jerry_create_number(GLCD_DS_BLINK_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_ON", val);

    val = jerry_create_number(GLCD_DS_BLINK_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_OFF", val);

    // input state flags
/*  This is not supported in Zephyr driver yet
    val = jerry_create_number(GLCD_IS_SHIFT_INCREMENT);
    zjs_set_property(glcd_obj, "GLCD_IS_SHIFT_INCREMENT", val);

    val = jerry_create_number(GLCD_IS_SHIFT_DECREMENT);
    zjs_set_property(glcd_obj, "GLCD_IS_SHIFT_DECREMENT", val);

    val = jerry_create_number(GLCD_IS_ENTRY_LEFT);
    zjs_set_property(glcd_obj, "GLCD_IS_ENTRY_LEFT", val);

    val = jerry_create_number(GLCD_IS_ENTRY_RIGHT);
    zjs_set_property(glcd_obj, "GLCD_IS_ENTRY_RIGHT", val);
*/

    // colors
    val = jerry_create_number(GROVE_RGB_WHITE);
    zjs_set_property(glcd_obj, "GROVE_RGB_WHITE", val);

    val = jerry_create_number(GROVE_RGB_RED);
    zjs_set_property(glcd_obj, "GROVE_RGB_RED", val);

    val = jerry_create_number(GROVE_RGB_GREEN);
    zjs_set_property(glcd_obj, "GROVE_RGB_GREEN", val);

    val = jerry_create_number(GROVE_RGB_BLUE);
    zjs_set_property(glcd_obj, "GROVE_RGB_BLUE", val);

    return glcd_obj;
}

void zjs_grove_lcd_cleanup()
{
    jerry_release_value(zjs_glcd_prototype);
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_GROVE_LCD
