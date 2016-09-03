// Copyright (c) 2016, Intel Corporation.

// Sample code for Arduino 101 send text to be displayed on the Grove LCD
// it will first blink the lcd display couple times when starting up
// then it will display a counter and rotate colors from white to red to green
// and then finally to blue repeatly

// Hardware Requirements:
//   - A Grove LCD
//   - pulldown resistors for SDA and SCL
// Wiring:
//   For LCD:
//     - Wire SDA on the LCD above the pulldown resistor. Connect that resistor to power (VCC)
//     - Wire SDL on the LCD above the pulldown resistor. Connect that resistor to ground (GND)
//     - Wire the SDA for the Arduino and place it between the wire for the LCD and the resistor
//     - Do the same for SDL
//     - Wire power and ground accordingly

// import grove_lcd module
var grove_lcd = require("grove_lcd");

var funcFlags = {
    GLCD_FS_8BIT_MODE : 1 << 4,
    GLCD_FS_ROWS_2 : 1 << 3,
    GLCD_FS_ROWS_1 : 0 << 3,
    GLCD_FS_DOT_SIZE_BIG : 1 << 2,
    GLCD_FS_DOT_SIZE_LITTLE : 0 << 2
};

var displayStateFlags = {
    GLCD_DS_DISPLAY_ON : 1 << 2,
    GLCD_DS_DISPLAY_OFF : 0 << 2,
    GLCD_DS_CURSOR_ON : 1 << 1,
    GLCD_DS_CURSOR_OFF : 0 << 1,
    GLCD_DS_BLINK_ON : 1 << 0,
    GLCD_DS_BLINK_OFF : 0 << 0
};

var inputStateFlags = {
    GLCD_IS_SHIFT_INCREMENT : 1 << 1,
    GLCD_IS_SHIFT_DECREMENT : 0 << 1,
    GLCD_IS_ENTRY_LEFT : 1 << 0,
    GLCD_IS_ENTRY_RIGHT : 0 << 0
};

var colors = {
    GROVE_RGB_WHITE : 0,
    GROVE_RGB_RED : 1,
    GROVE_RGB_GREEN : 2,
    GROVE_RGB_BLUE : 3
};

// set initial state
var funcConfig = funcFlags.GLCD_FS_ROWS_2
               | funcFlags.GLCD_FS_DOT_SIZE_LITTLE
               | funcFlags.GLCD_FS_8BIT_MODE;

var displayStateConfig = displayStateFlags.GLCD_DS_DISPLAY_ON;

var glcd = grove_lcd.init();

glcd.setFunction(funcConfig);
glcd.setDisplayState(displayStateConfig);

glcd.clear();
glcd.print("Grove LCD Sample");

// blink LCD
glcd.setColor(0, 0, 0);
glcd.setColor(255, 255, 255);
glcd.setColor(0, 0, 0);
glcd.setColor(255, 255, 255);

var counter = 0;
var color = colors.GROVE_RGB_WHITE;

setInterval(function () {
    var str = "Counter: " + counter;

    glcd.clear();
    glcd.setCursorPos(0, 0);
    glcd.print(str);
    glcd.setCursorPos(0, 1);

    if (color == colors.GROVE_RGB_WHITE)
        glcd.print("WHITE");
    else if (color == colors.GROVE_RGB_RED)
        glcd.print("RED");
    else if (color == colors.GROVE_RGB_GREEN)
        glcd.print("GREEN");
    else if (color == colors.GROVE_RGB_BLUE)
        glcd.print("BLUE");
    glcd.selectColor(color);

    // rotate color
    if (color == 3)
        color = 0;
    else
        color++;

    counter++;
}, 1000);
