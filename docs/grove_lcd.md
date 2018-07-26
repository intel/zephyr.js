ZJS API for Grove LCD
=====================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [GroveLCD API](#grovelcd-api)
  * [grove_lcd.init()](#grove_lcdinit)
* [GroveLCDDevice API](#grovelcddevice-api)
  * [groveLCDDevice.print(string text)](#grovelcddeviceprinttext)
  * [groveLCDDevice.clear()](#grovelcddeviceclear)
  * [groveLCDDevice.setCursorPos(col, row)](#grovelcddevicesetcursorposcol-row)
  * [groveLCDDevice.selectColor(index)](#grovelcddeviceselectcolorindex)
  * [groveLCDDevice.setColor(r, g, b)](#grovelcddevicesetcolorr-g-b)
  * [groveLCDDevice.setFunction(config)](#grovelcddevicesetfunctionconfig)
  * [groveLCDDevice.getFunction()](#grovelcddevicegetfunction)
  * [groveLCDDevice.setDisplayState(config)](#grovelcddevicesetdisplaystateconfig)
  * [GroveLCDDevice.getDisplayState()](#grovelcddevicegetdisplaystate)
* [Sample Apps](#sample-apps)

Introduction
------------
The Grove LCD API is the JavaScript version of the Zephyr API that supports the
Grove LCD.  It works over I2C to allow user to send text to the LCD screen
and also configure the LCD to different RGB backlight colors.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>// require returns a GroveLCD object
// var grove_lcd = require('grove_lcd');
[ReturnFromRequire]
interface GroveLCD {
    GroveLCDDevice init();
    attribute unsigned long GLCD_FS_8BIT_MODE;
    attribute unsigned long GLCD_FS_ROWS_2;
    attribute unsigned long GLCD_FS_ROWS_1;
    attribute unsigned long GLCD_FS_DOT_SIZE_BIG;
    attribute unsigned long GLCD_FS_DOT_SIZE_LITTLE;<p>
    attribute unsigned long GLCD_DS_DISPLAY_ON;
    attribute unsigned long GLCD_DS_DISPLAY_OFF;
    attribute unsigned long GLCD_DS_CURSOR_ON;
    attribute unsigned long GLCD_DS_CURSOR_OFF;
    attribute unsigned long GLCD_DS_BLINK_ON;
    attribute unsigned long GLCD_DS_BLINK_OFF;<p>    attribute unsigned long GLCD_IS_SHIFT_INCREMENT;
    attribute unsigned long GLCD_IS_SHIFT_DECREMENT;
    attribute unsigned long GLCD_IS_ENTRY_LEFT;
    attribute unsigned long GLCD_IS_ENTRY_RIGHT; <p>    attribute unsigned long GROVE_RGB_WHITE;
    attribute unsigned long GROVE_RGB_RED;
    attribute unsigned long GROVE_RGB_GREEN;
    attribute unsigned long GROVE_RGB_BLUE;
};<p>interface GroveLCDDevice {
    void print(string text);
    void clear();
    void setCursorPos(unsigned long col, unsigned long row);
    void selectColor(unsigned long index);
    void setColor(unsigned long r, unsigned long g, unsigned long b);
    void setFunction(unsigned long config);
    unsigned long getFunction();
    void setDisplayState(unsigned long config);
    unsigned long getDisplayState();
};</pre>
</details>

GroveLCD API
------------
### grove_lcd.init()
* Returns: a GroveLCDDevice object that can be used to
talk to the Grove LCD panel.

Initializes the Grove LCD panel.

*NOTE: Zephyr's Grove LCD API is on top of the I2C, which is only accessible
from the ARC side on the Arduino 101, so all the APIs in here will use the
IPM to send commands over to the API, and all this API will be synchronous.*

GroveLCDDevice API
------------------
### groveLCDDevice.print(text)
* `text` *string* The text to be printed.

Send text to the screen on the current line cursor is set to,
if the text is longer than number of characters it can fit on that line,
any additional characters will not wrap around and be dropped,
so a 16x2 LCD will have a maximum of 16 characters.

### groveLCDDevice.clear()

Clear the current display.

### groveLCDDevice.setCursorPos(col, row)
* `col` *unsigned long* The column for the cursor to be moved to (0-15).
* `row` *unsigned long* The row the column should be moved to (0 or 1).

Set text cursor position for the next print.

### groveLCDDevice.selectColor(index)
* `index` *unsigned long* The color selection, as defined, below.

Set LCD background to a predfined color.

The `index` should be a one of the following color selections:

GroveLCD.GROVE_RGB_WHITE

GroveLCD.GROVE_RGB_RED

GroveLCD.GROVE_RGB_GREEN

GroveLCD.GROVE_RGB_BLUE

### groveLCDDevice.setColor(r, g, b)
* `r` *unsigned long* The numeric value for the red color (max is 255).
* `g` *unsigned long* The numeric value for the green color (max is 255).
* `b` *unsigned long* The numeric value for the blue color (max is 255).

Set LCD background to custom RGB color value.

### groveLCDDevice.setFunction(config)
* `config` *unsigned long* The bit mask to change the display state, as described, below.

This function provides the user the ability to change the state
of the display, controlling things like the number of rows,
dot size, and text display quality.

The `config` bit mask can take the following configurations:

GroveLCD.GLCD_FS_8BIT_MODE

GroveLCD.GLCD_FS_ROWS_2

GroveLCD.GLCD_FS_ROWS_1

GroveLCD.GLCD_FS_DOT_SIZE_BIG

GroveLCD.GLCD_FS_DOT_SIZE_LITTLE

### groveLCDDevice.getFunction()
*Returns: the function-features set associated with the device.

### groveLCDDevice.setDisplayState(config)
* `config` *unsigned long* The bit mask to change the display state, as described, below.

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

### GroveLCDDevice.getDisplayState()
* Returns: the display-feature set associated with the device.

Sample Apps
-----------
* Grove LCD only
  * [Grove LCD sample](../samples/GroveLCD.js)
* Demos
  * [WebBluetooth Grove LCD Demo](../samples/WebBluetoothGroveLcdDemo.js)
  * [Heart Rate Demo](../samples/HeartRateDemo.js)
