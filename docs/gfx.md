ZJS API for GFX
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: GFX](#gfx-api)
  * [gfx.init(screen_width, screen_height, init_screen, draw, [this])](#gfxinitscreen_width-screen_height-init_screen-draw-this)
* [Class: GFXContext](#gfxcontext-api)
  * [gfxcontext.fillRect(x_coord, y_coord, width, height, color)](#gfxcontextfillrectx_coord-y_coord-width-height-color)
  * [gfxcontext.drawPixel(x_coord, y_coord, color)](#gfxcontextdrawpixelx_coord-y_coord-color)
  * [gfxcontext.drawLine(x0_coord, y0_coord, x1_coord, y1_coord, color, [size])](#gfxcontextdrawlinex0_coord-y0_coord-x1_coord-y1_coord-color-size)
  * [gfxcontext.drawVLine(x_coord, y_coord, height, color, [size])](#gfxcontextdrawvlinex_coord-y_coord-height-color-size)
  * [gfxcontext.drawHLine(x_coord, y_coord, width, color, [size])](#gfxcontextdrawhlinex_coord-y_coord-width-color-size)
  * [gfxcontext.drawRect(x_coord, y_coord, width, height, color, [size])](#gfxcontextdrawrectx_coord-y_coord-width-height-color-size)
  * [gfxcontext.drawChar(x_coord, y_coord, char, color, [size])](#gfxcontextdrawcharx_coord-y_coord-char-color-size)
  * [gfxcontext.drawString(x_coord, y_coord, str, color, [size])](#gfxcontextdrawstringx_coord-y_coord-str-color-size)
* [Sample Apps](#sample-apps)

Introduction
------------
The GFX module provides a generic way to create pixel buffers that can
be displayed on a display of some kind.  A JavaScript method for initializing
the screen and drawing a data buffer are required to use it.
See module/ST7735.js and samples/SPI_Screen.js for an example.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a GFX object
// var gfx = require('gfx');
[ReturnFromRequire]
interface GFX {
    GFXContext init(long screen_width, long screen_height, InitCallback init_screen,
                    DrawingCallback draw, optional this this_object);
};<p>
interface GFXContext {
    void fillRect(long x_coord, long y_coord, long width, long height,
                  sequence < byte > color);
    void drawPixel(long x_coord, long y_coord, sequence < byte > color);
    void drawLine(long x0_coord, long y0_coord, long x1_coord,
                  long y1_coord, sequence < byte > color, optional long size);
    void drawVLine(long x_coord, long y_coord, long height, sequence < byte > color,
                   optional long size);
    void drawHLine(long x_coord, long y_coord, long width, sequence < byte > color,
                   optional long size);
    void drawRect(long x_coord, long y_coord, long width, long height,
                  sequence < byte > color, optional long size);
    void drawChar(long x_coord, long y_coord, byte char, sequence < byte > color,
                  optional long size);
    void drawString(long x_coord, long y_coord, string str, sequence < byte > color,
                    optional long size);
};<p>callback InitCallback = void (any... params);
callback DrawingCallback = void (any... params);
</pre>
</details>

GFX API
-------
### gfx.init(screen_width, screen_height, init_screen, draw, this)
* `screen_width` *long* Width of the screen.
* `screen_height` *long* Height of the screen.
* `init_screen` *InitCallback*
* `draw` *DrawingCallback*
* `this` *object*

Initializes the GFX module with the screen size, an init function, and a draw
callback.  A 'this' object can also be provided if needed.

GFXContext API
--------------
### gfxcontext.fillRect(x_coord, y_coord, width, height, color)
* `x_coord` *long*
* `y_coord` *long*
* `width` *long*
* `height` *long*
* `color` *byte array*

Draws a solid rectangle of the given color at the coordinates provided.

### gfxcontext.drawPixel(x_coord, y_coord, color)
* `x_coord` *long*
* `y_coord` *long*
* `color` *byte array*

Draws a pixel of the given color at the coordinates provided.

### gfxcontext.drawLine(x0_coord, y0_coord, x1_coord, y1_coord, color, [size])
* `x0_coord` *long*
* `y0_coord` *long*
* `x1_coord` *long*
* `y1_coord` *long*
* `color` *byte array*
* `size` *long* Optional.


Draws a line of the given color at the coordinates provided.  The optional
size number controls how thick the line is.

### gfxcontext.drawVLine(x_coord, y_coord, height, color, [size])
* `x_coord` *long*
* `y_coord` *long*
* `height` *long*
* `color` *byte array*
* `size` *long* Optional.


Draws a vertical line of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### gfxcontext.drawHLine(x_coord, y_coord, width, color, [size])
* `x_coord` *long*
* `y_coord` *long*
* `width` *long*
* `color` *byte array*
* `size` *long* Optional.


Draws a horizontal line of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### gfxcontext.drawRect(x_coord, y_coord, width, height, color, [size])
* `x_coord` *long*
* `y_coord` *long*
* `width` *long*
* `height` *long*
* `color` *byte array*
* `size` *long* Optional.


Draws a hollow rectangle of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### gfxcontext.drawChar(x_coord, y_coord, char, color, [size])
* `x_coord` *long*
* `y_coord` *long*
* `char` *byte*
* `color` *byte array*
* `size` *long* Optional.


Draw a character at the coordinates given. The optional size number sets how
large the character is.

### gfxcontext.drawString(x_coord, y_coord, str, color, [size])
* `x_coord` *long*
* `y_coord` *long*
* `str` *string*
* `color` *byte array*
* `size` *long* Optional.


Draw a string at the coordinates given. The optional size number sets how
large the character is.

Sample Apps
-----------
* [GFX sample](../samples/SPI_Screen.js)
