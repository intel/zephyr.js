ZJS API for GFX
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The GFX module provides a generic way to create pixel buffers, which can then
be displayed on a display of some kind.  A JavaScript method for initializing
the screen, and drawing a data buffer are required to use it.
See module/ST7735.js and samples/SPI_Screen.js for an example.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a GFX object
// var gfx = require('gfx');

[NoInterfaceObject]
interface GFX {
    GFXContext init(screen width, screen height, init screen function,
                draw function, optional this);
};

[NoInterfaceObject]
interface GFXContext {
    fillRect(number x coord, number y coord, number width, number height,
             char array color);
    drawPixel(number x coord, number y coord, char array color);
    drawLine(number x0 coord, number y0 coord, number x1 coord,
             number y1 coord, char array color, optional number size);
    drawVLine(number x coord, number y coord, number height, char array color,
              optional number size);
    drawHLine(number x coord, number y coord, number width, char array color,
              optional number size);
    drawRect(number x coord, number y coord, number width, number height,
             char array color, optional number size);
    drawChar(number x coord, number y coord, character char, char array color,
             optional number size);
    drawString(number x coord, number y coord, string str, char array color,
               optional number size);
};
```

API Documentation
-----------------
### GFX.init

`GFXContext init(screen width, screen height, init screen function, draw function,
             optional this);`

Initializes the GFX module with the screen size, an init function, and a draw
callback.  A 'this' object can also be provided if needed.

### GFXContext.fillRect

`void drawPixel(number x coord, number y coord, char array color);`

Draws a solid rectangle of the given color at the coordinates provided.

### GFXContext.drawPixel

`void drawPixel(number x coord, number y coord, number width, number height,
                char array color);`

Draws a pixel of the given color at the coordinates provided.

### GFXContext.drawLine

`void drawLine(number x0 coord, number y0 coord, number x1 coord,
               number y1 coord, char array color, optional number size);`

Draws a line of the given color at the coordinates provided.  The optional
size number controls how thick the line is.

### GFXContext.drawVLine

`void drawVLine(number x coord, number y coord, number height, char array color,
                optional number size);`

Draws a vertical line of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### GFXContext.drawHLine

`void drawHLine(number x coord, number y coord, number width, char array color,
                optional number size);`

Draws a horizontal line of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### GFXContext.drawRect

`void drawRect(number x coord, number y coord, number width, number height,
               char array color, optional number size);`

Draws a hollow rectangle of the given color at the coordinates provided.  The
optional size number controls how thick the line is.

### GFXContext.drawChar

`void drawChar(number x coord, number y coord, character char, char array color,
               optional number size);`

Draw a character at the coordinates given. The optional size number sets how
large the character is.

### GFXContext.drawString

`void drawString(number x coord, number y coord, string str, char array color,
               optional number size);`

Draw a string at the coordinates given. The optional size number sets how
large the character is.

Sample Apps
-----------
* [GFX sample](../samples/SPI_Screen.js)
