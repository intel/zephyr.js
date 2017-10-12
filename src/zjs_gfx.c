// Copyright (c) 2017, Intel Corporation.

// This graphics library was inspired by the Adafruit GFX library
// https://github.com/adafruit/Adafruit-GFX-Library

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_error.h"
#include "zjs_gfx.h"
#include "zjs_util.h"
#include "zjs_gfx_font.h"

#define COLORBYTES 2

u16_t maxPixels = 128 * 160 * 3;

typedef struct gfx_handle {
    jerry_value_t screenInitCB;
    jerry_value_t drawDataCB;
    jerry_value_t jsThis;
} gfx_handle_t;

typedef struct gfx_data {
    u32_t coords[4];
    u8_t color[COLORBYTES];
    char *text;
    jerry_size_t textSize;
    u32_t size;
} gfx_data_t;

static jerry_value_t zjs_gfx_prototype;

static void zjs_gfx_callback_free(void *native)
{
    // requires: handle is the native pointer we registered with
    //             jerry_set_object_native_handle
    //  effects: frees the gfx module

    gfx_handle_t *handle = (gfx_handle_t *)native;
    zjs_free(handle);
}

static const jerry_object_native_info_t gfx_type_info = {
   .free_cb = zjs_gfx_callback_free
};

// Extracts the args from jerry values and puts them into the data struct
static void args_to_data(gfx_data_t *data, u32_t argc, const jerry_value_t argv[])
{
    // Init the struct
    for (u8_t i = 0; i < 4; i++)
        data->coords[i] = 0;

    data->size = 1;
    data->textSize = 0;

    if (argc < 7) {
        for (u8_t i = 0; i < argc; i++) {
            if (jerry_value_is_number(argv[i])) {
                if (i < 4)
                    data->coords[i] = (u32_t)jerry_get_number_value(argv[i]);
                else
                    data->size = (u32_t)jerry_get_number_value(argv[i]);
            }
            else if (jerry_value_is_string(argv[i])) {
                data->text = zjs_alloc_from_jstring(argv[i], &data->textSize);
                if (!data->text) {
                    ERR_PRINT ("GFX failed to copy text");
                }
            }
            else if (jerry_value_is_array(argv[i])) {
                for (u8_t j = 0; j < COLORBYTES; j++) {
                   data->color[j] = (u8_t)jerry_get_number_value(jerry_get_property_by_index(argv[i], j));
               }
            }
        }
    }
}

// Fills a buffer with the pixels to draw a solid rectangle and calls the provided JS callback to draw it on the screen
static jerry_value_t zjs_gfx_fill_rect_priv (u32_t x, u32_t y, u32_t w, u32_t h, u8_t color[], gfx_handle_t *gfxHandle)
{
    u32_t pixels = w * h * COLORBYTES; // Each pixel has several bytes of data
    u8_t passes = 1;    // Number of times the data buffer needs to be sent

    if (pixels > maxPixels) {
        pixels = maxPixels;
        passes = (pixels + (maxPixels -1)) / maxPixels;
    }

    zjs_buffer_t *recBuf = NULL;
    ZVAL recBufObj =  zjs_buffer_create(pixels, &recBuf);

    for (int i = 0; i < pixels; i++) {
        if (i % 2)
            recBuf->buffer[i] = color[1];
        else
            recBuf->buffer[i] = color[0];
    }

    jerry_value_t args[] = {jerry_create_number(x), jerry_create_number(y),
                            jerry_create_number(w), jerry_create_number(h), recBufObj};

    // Send the buffer as many times as needed to fill the rectangle
    for ( int i = 0; i < passes ; i++) {
        jerry_value_t ret = jerry_call_function(gfxHandle->drawDataCB, gfxHandle->jsThis, args, 5);
        if (jerry_value_has_error_flag (ret)) {
            return ret;
        }
    }
    return ZJS_UNDEFINED;
}

// Draws the rectangles needed to create the given char
static jerry_value_t zjs_gfx_draw_char_priv(u32_t x, u32_t y, char c, u8_t color[], u32_t size, gfx_handle_t *gfxHandle)
{
    u32_t asciiIndex = (u8_t)c - 33;    // To save size our font doesn't include the first 33 chars
    u8_t fontBytes = font_data_descriptors[asciiIndex][0] * 2;  // 2 bytes per pixel
    u16_t index = font_data_descriptors[asciiIndex][1];
    jerry_value_t ret = ZJS_UNDEFINED;
    zjs_buffer_t *charBuf = NULL;
    ZVAL charBufObj =  zjs_buffer_create(fontBytes, &charBuf);

    if (charBuf) {
        for (int i = 0; i < fontBytes; i++) {
            charBuf->buffer[i] = font_data[index + i];
        }
    }

    for(int i = 0; i < fontBytes; i+=2) {
        u16_t line = (((u16_t)charBuf->buffer[i]) << 8) | charBuf->buffer[i+1];
        for(int j = 0; j < 16; j++) {
            if((line >> j)  & 1) {
                int recX = x + (i/2) * size;
                int recY = y + j * size;
                // Draw each bit
                ret = zjs_gfx_fill_rect_priv(recX, recY, size, size, color, gfxHandle);

                if (jerry_value_has_error_flag (ret)) {
                    return ret;
                }
            }
        }
    }
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_gfx_fill_rect)
{
    // requires: Requires 5 arguments
    //           arg[0] - x coord of the top left.
    //           arg[1] - y coord of the top left.
    //           arg[2] - width.
    //           arg[3] - height.
    //           arg[4] - color array.
    //
    //  effects: Draws a filled rectangle on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_ARRAY);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    return zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], argData.coords[2], argData.coords[3], argData.color, handle);
}

static ZJS_DECL_FUNC(zjs_gfx_draw_pixel)
{
    // requires: Requires 3 arguments
    //           arg[0] - x coord of the top left.
    //           arg[1] - y coord of the top left.
    //           arg[2] - color array.
    //
    //  effects: Draws a pixel on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_ARRAY);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    return zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], 1, 1, argData.color, handle);
}

static ZJS_DECL_FUNC(zjs_gfx_draw_line)
{
    // requires: Requires 5 arguments
    //           arg[0] - x coord of the start.
    //           arg[1] - y coord of the start.
    //           arg[2] - x coord of the end.
    //           arg[3] - y coord of the end.
    //           arg[4] - color array.
    //           arg[5] - optional size of line.
    //
    //  effects: Draws a line on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    int xLen = argData.coords[2] > argData.coords[0] ? argData.coords[2] - argData.coords[0] : argData.coords[0] - argData.coords[2];
    int yLen = argData.coords[3] > argData.coords[1] ? argData.coords[3] - argData.coords[1] : argData.coords[1] - argData.coords[3];
    xLen = xLen == 0 ? 1 : xLen; // Line width has to be at least a pixel
    yLen = yLen == 0 ? 1 : yLen;
    bool neg = false;
    jerry_value_t ret = ZJS_UNDEFINED;

    if (xLen <= yLen) {
        // We always draw left to right, swap if argData.coords[0] is larger
        if (argData.coords[0] > argData.coords[2]) {
            u32_t tmp = argData.coords[0];
            argData.coords[0] = argData.coords[2];
            argData.coords[2] = tmp;
            tmp = argData.coords[1];
            argData.coords[1] = argData.coords[3];
            argData.coords[3] = tmp;
        }
        // Line is going up
        if (argData.coords[3] < argData.coords[1])
                neg = true;

        u32_t pos = argData.coords[1];
        int step = yLen / xLen;

        for (u32_t x = argData.coords[0]; x <= argData.coords[2]; x++) {
            ret = zjs_gfx_fill_rect_priv(x, pos, argData.size, step, argData.color, handle);
            if (jerry_value_has_error_flag (ret)){
                    return ret;
                }
            pos = neg == false ? pos + step : pos - step;
        }
    }
    else {
        // We always draw left to right, swap if argData.coords[1] is larger
        if (argData.coords[1] > argData.coords[3]) {
            u32_t tmp = argData.coords[0];
            argData.coords[0] = argData.coords[2];
            argData.coords[2] = tmp;
            tmp = argData.coords[1];
            argData.coords[1] = argData.coords[3];
            argData.coords[3] = tmp;
        }
        // Line is going up
        if (argData.coords[2] < argData.coords[0])
            neg = true;

        u32_t pos = argData.coords[0];
        int step = xLen / yLen;

        for (u32_t y = argData.coords[1]; y <= argData.coords[3]; y++) {
            ret = zjs_gfx_fill_rect_priv(pos, y, step, argData.size, argData.color, handle);
            if (jerry_value_has_error_flag (ret)) {
                    return ret;
                }
            pos = neg == false ? pos + step : pos - step;
        }
    }
    return ret;
}

static ZJS_DECL_FUNC(zjs_gfx_draw_v_line)
{
    // requires: Requires 4 arguments
    //           arg[0] - x coord of the start.
    //           arg[1] - y coord of the start.
    //           arg[2] - height of line.
    //           arg[3] - color array.
    //           arg[4] - optional size of line.
    //
    //  effects: Draws a vertical line on the screen.

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    return zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], argData.size, argData.coords[2], argData.color, handle);
}

static ZJS_DECL_FUNC(zjs_gfx_draw_h_line)
{
    // requires: Requires 4 arguments
    //           arg[0] - x coord of the start.
    //           arg[1] - y coord of the start.
    //           arg[2] - width of line.
    //           arg[3] - color array.
    //           arg[4] - optional size of line.
    //
    //  effects: Draws a horizontal line on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    return zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], argData.coords[2], argData.size, argData.color, handle);
}

static ZJS_DECL_FUNC(zjs_gfx_draw_rect)
{
    // requires: Requires 5 arguments
    //           arg[0] - x coord of the top left.
    //           arg[1] - y coord of the top left.
    //           arg[2] - width.
    //           arg[3] - height.
    //           arg[4] - color array.
    //           arg[5] - optional size of line.
    //
    //  effects: Draws a rectangle on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_NUMBER, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], argData.coords[2], argData.size, argData.color, handle);
    zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1] + argData.coords[3] - argData.size, argData.coords[2], argData.size, argData.color, handle);
    zjs_gfx_fill_rect_priv(argData.coords[0], argData.coords[1], argData.size, argData.coords[3], argData.color, handle);
    zjs_gfx_fill_rect_priv(argData.coords[0] + argData.coords[2] - argData.size, argData.coords[1], argData.size, argData.coords[3], argData.color, handle);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_gfx_draw_char)
{
    // requires: Requires 4 arguments
    //           arg[0] - x coord of the top left.
    //           arg[1] - y coord of the top left.
    //           arg[2] - character.
    //           arg[3] - color array.
    //           arg[4] - optional size of character.
    //
    //  effects: Draws a character on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_STRING, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    return zjs_gfx_draw_char_priv(argData.coords[0], argData.coords[1], argData.text[0], argData.color, argData.size, handle);
}

static ZJS_DECL_FUNC(zjs_gfx_draw_string) //(x, y, c, color, bg, size) {
{
    // requires: Requires 4 arguments
    //           arg[0] - x coord of the top left.
    //           arg[1] - y coord of the top left.
    //           arg[2] - text.
    //           arg[3] - color array.
    //           arg[4] - optional size of character.
    //
    //  effects: Draws a character on the screen

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_STRING, Z_ARRAY, Z_OPTIONAL Z_NUMBER);
    ZJS_GET_HANDLE(this, gfx_handle_t, handle, gfx_type_info);
    gfx_data_t argData;
    args_to_data(&argData, argc, argv);
    jerry_value_t ret = ZJS_UNDEFINED;

    for (u8_t i = 0; i < argData.textSize; i++) {
        ret = zjs_gfx_draw_char_priv(argData.coords[0] + (i * 7 * argData.size), argData.coords[1], argData.text[i], argData.color, argData.size, handle);
        if (jerry_value_has_error_flag (ret)) {
            return ret;
        }
    }
    return ret;
}

static ZJS_DECL_FUNC(zjs_gfx_set_cb)
{
    // requires: Requires 4 arguments
    //           arg[0] - The width of the screen.
    //           arg[1] - The hight of the screen.
    //           arg[2] - The init callback to use.
    //           arg[3] - The drawRect callback to use.
    //           arg[4] - optional JS 'this' object
    //
    //  effects: Initializes the GFX module

    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_FUNCTION, Z_FUNCTION, Z_OPTIONAL);

    gfx_handle_t *handle = zjs_malloc(sizeof(gfx_handle_t));

    if (!handle) {
        return ZJS_ERROR("could not allocate handle\n");
    }

    handle->screenInitCB = jerry_acquire_value(argv[2]);
    handle->drawDataCB = jerry_acquire_value(argv[3]);
    handle->jsThis = jerry_acquire_value(argv[4]);

    jerry_value_t gfx_obj = zjs_create_object();
    jerry_set_prototype(gfx_obj, zjs_gfx_prototype);
    jerry_set_object_native_pointer(gfx_obj, handle, &gfx_type_info);
    jerry_call_function(handle->screenInitCB, handle->jsThis, NULL, 0);
    return gfx_obj;
}

jerry_value_t zjs_gfx_init()
{
    zjs_native_func_t proto[] = {
        { zjs_gfx_fill_rect, "fillRect" },
        { zjs_gfx_draw_pixel, "drawPixel" },
        { zjs_gfx_draw_line, "drawLine" },
        { zjs_gfx_draw_v_line, "drawVLine" },
        { zjs_gfx_draw_h_line, "drawHLine" },
        { zjs_gfx_draw_rect, "drawRect" },
        { zjs_gfx_draw_char, "drawChar" },
        { zjs_gfx_draw_string, "drawString" },
        { NULL, NULL }
    };

    zjs_gfx_prototype = zjs_create_object();
    zjs_obj_add_functions(zjs_gfx_prototype, proto);

    jerry_value_t gfx_obj = zjs_create_object();
    //zjs_obj_add_function(gfx_obj, zjs_gfx_set_cb, "init");
    zjs_obj_add_function(gfx_obj, "init", zjs_gfx_set_cb);
    return gfx_obj;
}

void zjs_gfx_cleanup() {
    jerry_release_value(zjs_gfx_prototype);
}
