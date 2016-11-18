// Copyright (c) 2016, Intel Corporation.
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
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_GLCD_TIMEOUT_TICKS                      500

static struct k_sem glcd_sem;

static bool zjs_glcd_ipm_send_sync(zjs_ipm_message_t* send,
                                   zjs_ipm_message_t* result) {
    send->id = MSG_ID_GLCD;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_GLCD, send) != 0) {
        ERR_PRINT("zjs_glcd_ipm_send_sync: failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (k_sem_take(&glcd_sem, ZJS_GLCD_TIMEOUT_TICKS)) {
        ERR_PRINT("zjs_glcd_ipm_send_sync: FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static jerry_value_t zjs_glcd_call_remote_function(zjs_ipm_message_t* send)
{
    if (!send)
        return zjs_error("zjs_glcd_call_remote_function: invalid send message");

    zjs_ipm_message_t reply;

    bool success = zjs_glcd_ipm_send_sync(send, &reply);

    if (!success) {
        return zjs_error("zjs_glcd_call_remote_function: ipm message failed or timed out!");
    }

    if (reply.error_code != ERROR_IPM_NONE) {
        ERR_PRINT("error code: %lu\n", reply.error_code);
        return zjs_error("zjs_glcd_call_remote_function: error received");
    }

    uint8_t value = reply.data.glcd.value;

    return jerry_create_number(value);
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_GLCD)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t*)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
         zjs_ipm_message_t *result = (zjs_ipm_message_t*)msg->user_data;
        // synchrounus ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(zjs_ipm_message_t));

        // un-block sync api
        k_sem_give(&glcd_sem);
    } else {
        // asynchronous ipm, should not get here
        ERR_PRINT("ipm_msg_receive_callback: async message received\n");
    }
}

static jerry_value_t zjs_glcd_print(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(argv[0])) {
        return zjs_error("zjs_glcd_print: invalid argument");
    }

    jerry_size_t sz = jerry_get_string_size(argv[0]);

    char *buffer = zjs_malloc(sz+1);
    if (!buffer) {
        return zjs_error("zjs_glcd_print: cannot allocate buffer");
    }

    int len = jerry_string_to_char_buffer(argv[0],
                                          (jerry_char_t *)buffer,
                                          sz);
    buffer[len] = '\0';

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_PRINT;
    send.data.glcd.buffer = buffer;

    jerry_value_t result = zjs_glcd_call_remote_function(&send);
    zjs_free(buffer);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_clear(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_CLEAR;

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_cursor_pos(const jerry_value_t function_obj,
                                             const jerry_value_t this,
                                             const jerry_value_t argv[],
                                             const jerry_length_t argc)
{
    if (argc != 2 ||
        !jerry_value_is_number(argv[0]) ||
        !jerry_value_is_number(argv[1])) {
        return zjs_error("zjs_glcd_set_cursor_pos: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_CURSOR_POS;
    send.data.glcd.col = (uint8_t)jerry_get_number_value(argv[0]);
    send.data.glcd.row = (uint8_t)jerry_get_number_value(argv[1]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_select_color(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    if (argc != 1 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_glcd_select_color: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SELECT_COLOR;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_color(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc)
{
    if (argc != 3 ||
        !jerry_value_is_number(argv[0]) ||
        !jerry_value_is_number(argv[1]) ||
        !jerry_value_is_number(argv[2])) {
        return zjs_error("zjs_glcd_set_color: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_COLOR;
    send.data.glcd.color_r = (uint8_t)jerry_get_number_value(argv[0]);
    send.data.glcd.color_g = (uint8_t)jerry_get_number_value(argv[1]);
    send.data.glcd.color_b = (uint8_t)jerry_get_number_value(argv[2]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_set_function(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    if (argc != 1 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_glcd_set_function: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_FUNCTION;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_function(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_GET_FUNCTION;

    return zjs_glcd_call_remote_function(&send);
}

static jerry_value_t zjs_glcd_set_display_state(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    if (argc != 1 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_glcd_set_display_state: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_DISPLAY_STATE;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_display_state(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_GET_DISPLAY_STATE;

    return zjs_glcd_call_remote_function(&send);
}

static jerry_value_t zjs_glcd_set_input_state(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    if (argc != 1 || !jerry_value_is_number(argv[0])) {
        return zjs_error("zjs_glcd_set_input_state: invalid argument");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_INPUT_STATE;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_input_state(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_GET_INPUT_STATE;

    return zjs_glcd_call_remote_function(&send);
}

static jerry_value_t zjs_glcd_init(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_INIT;

    jerry_value_t result = zjs_glcd_call_remote_function(&send);

    if (jerry_value_has_error_flag(result)) {
        return result;
    }

    // create the Grove LCD device object
    jerry_value_t devObj = jerry_create_object();
    zjs_obj_add_function(devObj, zjs_glcd_print, "print");
    zjs_obj_add_function(devObj, zjs_glcd_clear, "clear");
    zjs_obj_add_function(devObj, zjs_glcd_set_cursor_pos, "setCursorPos");
    zjs_obj_add_function(devObj, zjs_glcd_select_color, "selectColor");
    zjs_obj_add_function(devObj, zjs_glcd_set_color, "setColor");
    zjs_obj_add_function(devObj, zjs_glcd_set_function, "setFunction");
    zjs_obj_add_function(devObj, zjs_glcd_get_function, "getFunction");
    zjs_obj_add_function(devObj, zjs_glcd_set_display_state, "setDisplayState");
    zjs_obj_add_function(devObj, zjs_glcd_get_display_state, "getDisplayState");
    zjs_obj_add_function(devObj, zjs_glcd_set_input_state, "setInputState");
    zjs_obj_add_function(devObj, zjs_glcd_get_input_state, "getInputState");

    return devObj;
}

jerry_value_t zjs_grove_lcd_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_GLCD, ipm_msg_receive_callback);

    k_sem_init(&glcd_sem, 0, 1);

    // create global grove_lcd object
    jerry_value_t glcd_obj = jerry_create_object();
    zjs_obj_add_function(glcd_obj, zjs_glcd_init, "init");

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

    // input state flags
    val = jerry_create_number(GLCD_IS_SHIFT_INCREMENT);
    zjs_set_property(glcd_obj, "GLCD_IS_SHIFT_INCREMENT", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_IS_SHIFT_DECREMENT);
    zjs_set_property(glcd_obj, "GLCD_IS_SHIFT_DECREMENT", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_IS_ENTRY_LEFT);
    zjs_set_property(glcd_obj, "GLCD_IS_ENTRY_LEFT", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_IS_ENTRY_RIGHT);
    zjs_set_property(glcd_obj, "GLCD_IS_ENTRY_RIGHT", val);
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

    return glcd_obj;
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_GROVE_LCD
