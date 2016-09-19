// Copyright (c) 2016, Intel Corporation.

// Sample code for Arduino 101 send text to be displayed on the Grove LCD
// it will first blink the lcd display couple times when starting up
// then it will display a counter and rotate colors from white to red to green
// and then finally to blue repeatly

// Hardware Requirements:
//   - A Grove LCD
//   - Two pull-up resistors for SDA and SCL, we use two 10k resistors,
//       you should choose resistors that will work with the LCD hardware
//       you have, the ones we listed here are the ones that have known
//       to work for us, so your mileage may vary if you have different LCD
// Wiring:
//   For the LCD:
//     - Wire SDA on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD to SDA on the Arduino 101
//     - Wire SCL on the LCD to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

var grove_lcd = require("grove_lcd");

// set initial state
var funcConfig = grove_lcd.GLCD_FS_ROWS_2
               | grove_lcd.GLCD_FS_DOT_SIZE_LITTLE
               | grove_lcd.GLCD_FS_8BIT_MODE;

var displayStateConfig = grove_lcd.GLCD_DS_DISPLAY_ON;

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
var color = grove_lcd.GROVE_RGB_WHITE;

setInterval(function () {
    var str = "Counter: " + counter;

    glcd.clear();
    glcd.setCursorPos(0, 0);
    glcd.print(str);
    glcd.setCursorPos(0, 1);

    if (color == grove_lcd.GROVE_RGB_WHITE)
        glcd.print("WHITE");
    else if (color == grove_lcd.GROVE_RGB_RED)
        glcd.print("RED");
    else if (color == grove_lcd.GROVE_RGB_GREEN)
        glcd.print("GREEN");
    else if (color == grove_lcd.GROVE_RGB_BLUE)
        glcd.print("BLUE");
    glcd.selectColor(color);

    // rotate color
    if (color == 3)
        color = 0;
    else
        color++;

    counter++;
}, 1000);
