// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_GROVE_LCD
#ifndef QEMU_BUILD

// C includes
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif
#include <device.h>
#include <display/grove_lcd.h>
#include <misc/util.h>

// ZJS includes
#include "zjs_util.h"

#define MAX_BUFFER_SIZE 256

static struct device *glcd = NULL;

static jerry_value_t zjs_glcd_prototype;

static ZJS_DECL_FUNC(zjs_glcd_print)
{
    // args: text
    ZJS_VALIDATE_ARGS(Z_STRING);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    jerry_size_t size = MAX_BUFFER_SIZE;
    char *buffer = zjs_alloc_from_jstring(argv[0], &size);
    if (!buffer) {
        return zjs_error("cannot allocate buffer");
    }

    glcd_print(glcd, buffer, size);
    DBG_PRINT("Grove LCD print: %s\n", buffer);
    zjs_free(buffer);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_clear)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    glcd_clear(glcd);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_set_cursor_pos)
{
    // args: column, row
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t col = (u8_t)jerry_get_number_value(argv[0]);
    u8_t row = (u8_t)jerry_get_number_value(argv[1]);
    glcd_cursor_pos_set(glcd, col, row);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_select_color)
{
    // args: predefined color index
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t value = (u8_t)jerry_get_number_value(argv[0]);
    glcd_color_select(glcd, value);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_set_color)
{
    // args: red, green, blue
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t r = (u8_t)jerry_get_number_value(argv[0]);
    u8_t g = (u8_t)jerry_get_number_value(argv[1]);
    u8_t b = (u8_t)jerry_get_number_value(argv[2]);
    glcd_color_set(glcd, r, g, b);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_set_function)
{
    // args: predefined function number
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t value = (u8_t)jerry_get_number_value(argv[0]);
    glcd_function_set(glcd, value);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_get_function)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t value = glcd_function_get(glcd);

    return jerry_create_number(value);
}

static ZJS_DECL_FUNC(zjs_glcd_set_display_state)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t value = (u8_t)jerry_get_number_value(argv[0]);
    glcd_display_state_set(glcd, value);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_glcd_get_display_state)
{
    if (!glcd) {
        return zjs_error("Grove LCD device not found");
    }

    u8_t value = glcd_display_state_get(glcd);

    return jerry_create_number(value);
}

static ZJS_DECL_FUNC(zjs_glcd_init)
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
    jerry_value_t dev_obj = zjs_create_object();
    jerry_set_prototype(dev_obj, zjs_glcd_prototype);
    return dev_obj;
}

static void zjs_grove_lcd_cleanup(void *native)
{
    jerry_release_value(zjs_glcd_prototype);
}

static const jerry_object_native_info_t grove_lcd_module_type_info = {
    .free_cb = zjs_grove_lcd_cleanup
};

// Note. setInputState is not supported in Zephyr driver yet
// with right-to-left text flow (GLCD_IS_SHIFT_DECREMENT|GLCD_IS_ENTRY_RIGHT)
// and defaults to left-to-right only, so we don't support
// configuring input state until Zephyr implements this feature
static jerry_value_t zjs_grove_lcd_init()
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
        { NULL, NULL }
    };
    zjs_glcd_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_glcd_prototype, array);

    // create global grove_lcd object
    jerry_value_t glcd_obj = zjs_create_object();
    zjs_obj_add_function(glcd_obj, "init", zjs_glcd_init);

    // create object properties
    jerry_value_t val;

    // function flags
    val = jerry_create_number(GLCD_FS_8BIT_MODE);
    zjs_set_property(glcd_obj, "GLCD_FS_8BIT_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_ROWS_2);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_2", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_ROWS_1);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_1", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_BIG);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_BIG", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_LITTLE);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_LITTLE", val);
    jerry_release_value(val);

    // display state flags
    val = jerry_create_number(GLCD_DS_DISPLAY_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_DISPLAY_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_OFF", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_CURSOR_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_CURSOR_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_OFF", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_BLINK_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_BLINK_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_OFF", val);
    jerry_release_value(val);

    // colors
    val = jerry_create_number(GROVE_RGB_WHITE);
    zjs_set_property(glcd_obj, "GROVE_RGB_WHITE", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_RED);
    zjs_set_property(glcd_obj, "GROVE_RGB_RED", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_GREEN);
    zjs_set_property(glcd_obj, "GROVE_RGB_GREEN", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_BLUE);
    zjs_set_property(glcd_obj, "GROVE_RGB_BLUE", val);
    jerry_release_value(val);

    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(glcd_obj, NULL,
                                    &grove_lcd_module_type_info);
    return glcd_obj;
}

JERRYX_NATIVE_MODULE(grove_lcd, zjs_grove_lcd_init)
#endif  // QEMU_BUILD
#endif  // BUILD_MODULE_GROVE_LCD
