// Copyright (c) 2017, Intel Corporation.

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_error.h"
#include "zjs_gfx.h"
#include "zjs_util.h"

#define COLORBYTES 2

u8_t font_data[] = {0,0,0,0,0,62,91,79,91,62,62,107,79,107,62,28,62,124,62,28,
                    24,60,126,60,24,28,87,125,87,28,28,94,127,94,28,0,24,60,24,
                    0,255,231,195,231,255,0,24,36,24,0,255,231,219,231,255,48,
                    72,58,6,14,38,41,121,41,38,64,127,5,5,7,64,127,5,37,63,90,
                    60,231,60,90,127,62,28,28,8,8,28,28,62,127,20,34,127,34,20,
                    95,95,0,95,95,6,9,127,1,127,0,102,137,149,106,96,96,96,96,
                    96,148,162,255,162,148,8,4,126,4,8,16,32,126,32,16,8,8,42,
                    28,8,8,28,42,8,8,30,16,16,16,16,12,30,12,30,12,48,56,62,56,
                    48,6,14,62,14,6,0,0,0,0,0,0,0,95,0,0,0,7,0,7,0,20,127,20,
                    127,20,36,42,127,42,18,35,19,8,100,98,54,73,86,32,80,0,8,7,
                    3,0,0,28,34,65,0,0,65,34,28,0,42,28,127,28,42,8,8,62,8,8,0,
                    128,112,48,0,8,8,8,8,8,0,0,96,96,0,32,16,8,4,2,62,81,73,69,
                    62,0,66,127,64,0,114,73,73,73,70,33,65,73,77,51,24,20,18,
                    127,16,39,69,69,69,57,60,74,73,73,49,65,33,17,9,7,54,73,73,
                    73,54,70,73,73,41,30,0,0,20,0,0,0,64,52,0,0,0,8,20,34,65,20,
                    20,20,20,20,0,65,34,20,8,2,1,89,9,6,62,65,93,89,78,124,18,
                    17,18,124,127,73,73,73,54,62,65,65,65,34,127,65,65,65,62,
                    127,73,73,73,65,127,9,9,9,1,62,65,65,81,115,127,8,8,8,127,
                    0,65,127,65,0,32,64,65,63,1,127,8,20,34,65,127,64,64,64,64,
                    127,2,28,2,127,127,4,8,16,127,62,65,65,65,62,127,9,9,9,6,62,
                    65,81,33,94,127,9,25,41,70,38,73,73,73,50,3,1,127,1,3,63,64,
                    64,64,63,31,32,64,32,31,63,64,56,64,63,99,20,8,20,99,3,4,
                    120,4,3,97,89,73,77,67,0,127,65,65,65,2,4,8,16,32,0,65,65,
                    65,127,4,2,1,2,4,64,64,64,64,64,0,3,7,8,0,32,84,84,120,64,
                    127,40,68,68,56,56,68,68,68,40,56,68,68,40,127,56,84,84,84,
                    24,0,8,126,9,2,24,164,164,156,120,127,8,4,4,120,0,68,125,64,
                    0,32,64,64,61,0,127,16,40,68,0,0,65,127,64,0,124,4,120,4,
                    120,124,8,4,4,120,56,68,68,68,56,252,24,36,36,24,24,36,36,
                    24,252,124,8,4,4,8,72,84,84,84,36,4,4,63,68,36,60,64,64,32,
                    124,28,32,64,32,28,60,64,48,64,60,68,40,16,40,68,76,144,144,
                    144,124,68,100,84,76,68,0,8,54,65,0,0,0,119,0,0,0,65,54,8,
                    0,2,1,2,4,2,60,38,35,38,60,30,161,161,97,18,58,64,64,32,122,
                    56,84,84,85,89,33,85,85,121,65,34,84,84,120,66,33,85,84,120,
                    64,32,84,85,121,64,12,30,82,114,18,57,85,85,85,89,57,84,84,
                    84,89,57,85,84,84,88,0,0,69,124,65,0,2,69,125,66,0,1,69,124,
                    64,125,18,17,18,125,240,40,37,40,240,124,84,85,69,0,32,84,
                    84,124,84,124,10,9,127,73,50,73,73,73,50,58,68,68,68,58,50,
                    74,72,72,48,58,65,65,33,122,58,66,64,32,120,0,157,160,160,
                    125,61,66,66,66,61,61,64,64,64,61,60,36,255,36,36,72,126,73,
                    67,102,43,47,252,47,43,255,9,41,246,32,192,136,126,9,3,32,
                    84,84,121,65,0,0,68,125,65,48,72,72,74,50,56,64,64,34,122,
                    0,122,10,10,114,125,13,25,49,125,38,41,41,47,40,38,41,41,41,
                    38,48,72,77,64,32,56,8,8,8,8,8,8,8,8,56,47,16,200,172,186,
                    47,16,40,52,250,0,0,123,0,0,8,20,42,20,34,34,20,42,20,8,85,
                    0,85,0,85,170,85,170,85,170,255,85,255,85,255,0,0,0,255,0,
                    16,16,16,255,0,20,20,20,255,0,16,16,255,0,255,16,16,240,16,
                    240,20,20,20,252,0,20,20,247,0,255,0,0,255,0,255,20,20,244,
                    4,252,20,20,23,16,31,16,16,31,16,31,20,20,20,31,0,16,16,16,
                    240,0,0,0,0,31,16,16,16,16,31,16,16,16,16,240,16,0,0,0,255,
                    16,16,16,16,16,16,16,16,16,255,16,0,0,0,255,20,0,0,255,0,
                    255,0,0,31,16,23,0,0,252,4,244,20,20,23,16,23,20,20,244,4,
                    244,0,0,255,0,247,20,20,20,20,20,20,20,247,0,247,20,20,20,
                    23,20,16,16,31,16,31,20,20,20,244,20,16,16,240,16,240,0,0,
                    31,16,31,0,0,0,31,20,0,0,0,252,20,0,0,240,16,240,16,16,255,
                    16,255,20,20,20,255,20,16,16,16,31,0,0,0,0,240,16,255,255,
                    255,255,255,240,240,240,240,240,255,255,255,0,0,0,0,0,255,
                    255,15,15,15,15,15,56,68,68,56,68,252,74,74,74,52,126,2,2,
                    6,6,2,126,2,126,2,99,85,73,65,99,56,68,68,60,4,64,126,32,30,
                    32,6,2,126,2,2,153,165,231,165,153,28,42,73,42,28,76,114,1,
                    114,76,48,74,77,77,48,48,72,120,72,48,188,98,90,70,61,62,73,
                    73,73,0,126,1,1,1,126,42,42,42,42,42,68,68,95,68,68,64,81,
                    74,68,64,64,68,74,81,64,0,0,255,1,3,224,128,255,0,0,8,8,107,
                    107,8,54,18,54,36,54,6,15,9,15,6,0,0,24,24,0,0,0,16,16,0,48,
                    64,255,1,1,0,31,1,1,30,0,25,29,23,18,0,60,60,60,60,0,0,0,0,0};

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
    u32_t index = (u8_t)c;
    index *= 5;
    jerry_value_t ret = ZJS_UNDEFINED;
    zjs_buffer_t *charBuf = NULL;
    ZVAL charBufObj =  zjs_buffer_create(5, &charBuf);

    if (charBuf) {
        for (int i = 0; i < 5; i++) {
            charBuf->buffer[i] = font_data[index + i];
        }
    }

    for(int i = 0; i < 5; i++) { // Char bitmap == 5 columns
        char line = charBuf->buffer[i];
        for(int j = 0; j < 8; j++) {    // Draw each bit
            if((line >> j)  & 1) {
                int recX = x + i * size;
                int recY = y + j * size;
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
            u32_t tmp = argData.coords[0]; argData.coords[0] = argData.coords[2]; argData.coords[2] = tmp;
            tmp = argData.coords[1]; argData.coords[1] = argData.coords[3]; argData.coords[3] = tmp;
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
            u32_t tmp = argData.coords[0]; argData.coords[0] = argData.coords[2]; argData.coords[2] = tmp;
            tmp = argData.coords[1]; argData.coords[1] = argData.coords[3]; argData.coords[3] = tmp;
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
        ret = zjs_gfx_draw_char_priv(argData.coords[0] + (i * 6 * argData.size), argData.coords[1], argData.text[i], argData.color, argData.size, handle);
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
