ZJS API for Grove LCD
=====================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The Grove LCD API is the JavaScript version of the Zephyr API that supports the
Grove LCD.  It works over I2C to allow user to send text to the LCD screen
and also configure LCD to different RGB backlight colors.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a GroveLCD object
// var grove_lcd = require('grove_lcd');

[NoInterfaceObject]
interface GroveLCD {
    GroveLCDDevice init();
    unsigned long GLCD_FS_8BIT_MODE;
    unsigned long GLCD_FS_ROWS_2;
    unsigned long GLCD_FS_ROWS_1;
    unsigned long GLCD_FS_DOT_SIZE_BIG;
    unsigned long GLCD_FS_DOT_SIZE_LITTLE;

    unsigned long GLCD_DS_DISPLAY_ON;
    unsigned long GLCD_DS_DISPLAY_OFF;
    unsigned long GLCD_DS_CURSOR_ON;
    unsigned long GLCD_DS_CURSOR_OFF;
    unsigned long GLCD_DS_BLINK_ON;
    unsigned long GLCD_DS_BLINK_OFF;

    unsigned long GLCD_IS_SHIFT_INCREMENT;
    unsigned long GLCD_IS_SHIFT_DECREMENT;
    unsigned long GLCD_IS_ENTRY_LEFT;
    unsigned long GLCD_IS_ENTRY_RIGHT;

    unsigned long GROVE_RGB_WHITE;
    unsigned long GROVE_RGB_RED;
    unsigned long GROVE_RGB_GREEN;
    unsigned long GROVE_RGB_BLUE;
};

[NoInterfaceObject]
interface GroveLCDDevice {
    void print(string text);
    void clear();
    void setCursorPos(unsigned long col, unsigned long row);
    void selectColor(unsigned long index);
    void setColor(unsigned long r, unsigned long g, unsigned long b);
    void setFunction(unsigned long config);
    unsigned long getFunction();
    void setDisplayState(unsigned long config);
    unsigned long getDisplayState();
    void setInputState(unsigned long config);
    unsigned long getInputState();
};
```

API Documentation
-----------------
### GroveLCD.init

`GroveLCDDevice init();`

Initialize the Grove LCD panel

*NOTE: Zephyr's Grove LCD API is on top of the I2C which is only accessible
from the ARC side on the Arduino 101, so all the API in here will use the
IPM to send commands over to the API, and all the API will be synchronous*

The function returns a GroveLCDDevice object instance that can be used to
talk to the Grove LCD panel.

### GroveLCDDevice.print

`void print(string text);`

Send text to the screen on the current line cursor is set to,
if the text is longer than number of characters it can fit on that line,
any additional characters will not wrap around and be dropped,
so a 16x2 LCD will have a maximum of 16 characters.

### GroveLCDDevice.clear

`void clear();`

Clear the current display.

### GroveLCDDevice.setCursorPos

`void setCursorPos(unsigned long col, unsigned long row);`

Set text cursor position for next additions.
The `col` is the column for the cursor to be moved to (0-15).
The `row` is the row it should be moved to (0 or 1).

### GroveLCDDevice.selectColor

`void selectColor(unsigned long index);`

Set LCD background to a predfined color.

The `index` should be a one of the following color selections:

GroveLCD.GROVE_RGB_WHITE

GroveLCD.GROVE_RGB_RED

GroveLCD.GROVE_RGB_GREEN

GroveLCD.long GROVE_RGB_BLUE

### GroveLCDDevice.setColor

`void setColor(unsigned long r, unsigned long g, unsigned long b);`

Set LCD background to custom RGB color value

The `r` is a numeric value for the red color (max is 255).
The `g` is a numeric value for the green color (max is 255).
The `b` is a numeric value for the blue color (max is 255).

### GroveLCDDevice.setFunction

`void setFunction(unsigned long config);`

This function provides the user the ability to change the state
of the display, controlling things like the number of rows,
dot size, and text display quality.

The `config` is bit mask of the following configurations:

GroveLCD.GLCD_FS_8BIT_MODE

GroveLCD.GLCD_FS_ROWS_2

GroveLCD.GLCD_FS_ROWS_1

GroveLCD.GLCD_FS_DOT_SIZE_BIG

GroveLCD.GLCD_FS_DOT_SIZE_LITTLE

### GroveLCDDevice.getFunction

`unsigned long getFunction();`

Return the function features set associated with the device.

### GroveLCDDevice.setDisplayState

`void setDisplayState(unsigned long config);`

This function provides the user the ability to change the state
of the display, controlling things like powering on or off
the screen, the option to display the cursor or not, and the ability to
blink the cursor.

The `config` is bit mask of the following configurations:

GroveLCD.GLCD_DS_DISPLAY_ON

GroveLCD.GLCD_DS_DISPLAY_OFF

GroveLCD.GLCD_DS_CURSOR_ON

GroveLCD.GLCD_DS_CURSOR_OFF

GroveLCD.GLCD_DS_BLINK_ON

GroveLCD.GLCD_DS_BLINK_OFF

### GroveLCDDevice.getDisplayState

`unsigned long getDisplayState();`

Return the display feature set associated with the device.

### GroveLCDDevice.setInputState

`void setInputState(unsigned long config);`

This function provides the user the ability to change the state
of the text input. Controlling things like text entry from the left or
right side, and how far to increment on new text.

The `config` is bit mask of the following configurations:

GroveLCD.GLCD_IS_SHIFT_INCREMENT

GroveLCD.GLCD_IS_SHIFT_DECREMENT

GroveLCD.GLCD_IS_ENTRY_LEFT

GroveLCD.GLCD_IS_ENTRY_RIGHT

### GroveLCDDevice.getInputState

`unsigned long getInputState();`

Return the input set associated with the device.

Sample Apps
-----------
* Grove LCD only
  * [Grove LCD sample](../samples/GroveLCD.js)
* Demos
  * [WebBluetooth Grove LCD Demo](../samples/WebBluetoothGroveLcdDemo.js)
  * [Heart Rate Demo](../samples/HeartRateDemo.js)
