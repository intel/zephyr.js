// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_GROVE_LCD
#ifndef QEMU_BUILD
// Zephyr includes
#include <misc/util.h>
#include <string.h>

// ZJS includes
#include "zjs_grove_lcd.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_GLCD_TIMEOUT_TICKS                      500

static struct nano_sem glcd_sem;


static struct zjs_ipm_message* zjs_glcd_alloc_msg()
{
    struct zjs_ipm_message *msg = task_malloc(sizeof(struct zjs_ipm_message));
    if (!msg) {
        PRINT("zjs_glcd_alloc_msg: cannot allocate message\n");
        return NULL;
    } else {
        memset(msg, 0, sizeof(struct zjs_ipm_message));
    }

    msg->id = MSG_ID_GLCD;
    msg->flags = 0 | MSG_SAFE_TO_FREE_FLAG;
    msg->error_code = ERROR_IPM_NONE;
    return msg;
}

static void zjs_glcd_free_msg(struct zjs_ipm_message* msg)
{
    if (!msg)
        return;

    if ((msg->flags & MSG_SAFE_TO_FREE_FLAG) == MSG_SAFE_TO_FREE_FLAG) {
        task_free(msg);
    } else {
        PRINT("zjs_glcd_free_msg: error! do not free message\n");
    }
}

static bool zjs_glcd_ipm_send_sync(struct zjs_ipm_message* send,
                                   struct zjs_ipm_message* result) {
    send->flags |= MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_GLCD, send) != 0) {
        PRINT("zjs_glcd_ipm_send_sync: failed to send message\n");
        return false;
    }

    // block until reply or timeout
    if (!nano_sem_take(&glcd_sem, ZJS_GLCD_TIMEOUT_TICKS)) {
        PRINT("zjs_glcd_ipm_send_sync: ipm timed out\n");
        return false;
    }

    return true;
}

static jerry_value_t zjs_glcd_call_remote_function(struct zjs_ipm_message* send)
{
    if (!send)
        return zjs_error("zjs_glcd_call_remote_function: invalid send message");

    struct zjs_ipm_message* reply = zjs_glcd_alloc_msg();;

    bool success = zjs_glcd_ipm_send_sync(send, reply);
    zjs_glcd_free_msg(send);

    if (!success) {
        zjs_glcd_free_msg(reply);
        return zjs_error("zjs_glcd_print: ipm message failed or timed out!");
    }

    if (reply->error_code != ERROR_IPM_NONE) {
        PRINT("zjs_glcd_print: error code: %lu\n", reply->error_code);
        zjs_glcd_free_msg(reply);
        return zjs_error("zjs_glcd_print: error received");
    }

    uint8_t value = reply->data.glcd.value;
    zjs_glcd_free_msg(reply);

    return jerry_create_number(value);
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_GLCD)
        return;

    struct zjs_ipm_message *msg = (struct zjs_ipm_message*)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
         struct zjs_ipm_message *result = (struct zjs_ipm_message*)msg->user_data;
        // synchrounus ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(struct zjs_ipm_message));

        // un-block sync api
        nano_isr_sem_give(&glcd_sem);
    } else {
        // asynchronous ipm, should not get here
        PRINT("ipm_msg_receive_callback: async message received\n");
    }
}

static jerry_value_t zjs_glcd_print(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    if (argc < 1 || !jerry_value_is_string(argv[0])) {
        PRINT("zjs_glcd_print: invalid argument\n");
        return zjs_error("zjs_glcd_print: invalid argument");
    }

    jerry_size_t sz = jerry_get_string_size(argv[0]);

    char *buffer = task_malloc(sz+1);
    if (!buffer) {
        PRINT("zjs_glcd_print: cannot allocate buffer\n");
        return zjs_error("cannot allocate buffer");
    }

    int len = jerry_string_to_char_buffer(argv[0],
                                          (jerry_char_t *)buffer,
                                          sz);
    buffer[len] = '\0';

    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_PRINT;
    send->data.glcd.buffer = buffer;

    jerry_value_t result = zjs_glcd_call_remote_function(send);
    task_free(buffer);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_clear(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc)
{
    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    // no input parameter to set
    send->type = TYPE_GLCD_CLEAR;

    jerry_value_t result = zjs_glcd_call_remote_function(send);

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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SET_CURSOR_POS;
    send->data.glcd.col = (uint8_t)jerry_get_number_value(argv[0]);
    send->data.glcd.row = (uint8_t)jerry_get_number_value(argv[1]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SELECT_COLOR;
    send->data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SET_COLOR;
    send->data.glcd.color_r = (uint8_t)jerry_get_number_value(argv[0]);
    send->data.glcd.color_g = (uint8_t)jerry_get_number_value(argv[1]);
    send->data.glcd.color_b = (uint8_t)jerry_get_number_value(argv[2]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SET_FUNCTION;
    send->data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_function(const jerry_value_t function_obj,
                                           const jerry_value_t this,
                                           const jerry_value_t argv[],
                                           const jerry_length_t argc)
{
    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    // no input parameter to set
    send->type = TYPE_GLCD_GET_FUNCTION;

    return zjs_glcd_call_remote_function(send);
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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SET_DISPLAY_STATE;
    send->data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_display_state(const jerry_value_t function_obj,
                                                const jerry_value_t this,
                                                const jerry_value_t argv[],
                                                const jerry_length_t argc)
{
    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    // no input parameter to set
    send->type = TYPE_GLCD_GET_DISPLAY_STATE;

    return zjs_glcd_call_remote_function(send);
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
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    send->type = TYPE_GLCD_SET_INPUT_STATE;
    send->data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    jerry_value_t result = zjs_glcd_call_remote_function(send);

    return jerry_value_has_error_flag(result) ? result : ZJS_UNDEFINED;
}

static jerry_value_t zjs_glcd_get_input_state(const jerry_value_t function_obj,
                                              const jerry_value_t this,
                                              const jerry_value_t argv[],
                                              const jerry_length_t argc)
{
    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    // no input parameter to set
    send->type = TYPE_GLCD_GET_INPUT_STATE;

    return zjs_glcd_call_remote_function(send);
}

static jerry_value_t zjs_glcd_init(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc)
{
    // send IPM message to the ARC side
    struct zjs_ipm_message* send = zjs_glcd_alloc_msg();
    // no input parameter to set
    send->type = TYPE_GLCD_INIT;

    jerry_value_t result = zjs_glcd_call_remote_function(send);

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

    nano_sem_init(&glcd_sem);

    // create global grove_lcd object
    jerry_value_t glcd_obj = jerry_create_object();
    zjs_obj_add_function(glcd_obj, zjs_glcd_init, "init");
    return glcd_obj;
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_GROVE_LCD
