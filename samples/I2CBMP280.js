// Copyright (c) 2016, Intel Corporation.

// Sample code demonstrating how to use I2C to communicate with slave devices

// Hardware Requirements:
//   - A BMP280 Temp/pressure sensor
//   - pull-up resistors for SDA and SCL, use two 10k resistors

// Wiring:
//   For LCD and BMP280:
//     - Wire SDA on the Arduino 101 to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the Arduino 101 to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD and BMP280 to SDA on the Arduino 101
//     - Wire SCL on the LCD and BMP280 to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

console.log("Starting I2C demo using BMP280 and Grove LCD");

/* Globals */

// Import JavaScript modules from 'modules' folder
var bmp280 = require("BMP280.js");
var glcd = require("GroveLCD.js");

var baseTemp = prevTemp = 25;
var red = green = blue = 200;

// Init the screen with text
glcd.writeText("Temperature is ", 0, 0);

function setColorFromTemperature(newTemp) {
    // If newTemp is higher than prevTemp change color to be
    // more red.  If lower change it to be more blue.
    var diff = newTemp - baseTemp;
    var change = (diff > 0) ? (diff * 40) : (diff * -40);
    change = (change > 255) ? 255 : (change | 0);

    if (baseTemp < newTemp) {
        blue = (200 - change < 0) ? 0 : (200 - change);
        green = blue;
        red = (200 + change > 255) ? 255 : (200 + change);
    } else {
        red = (200 - change < 0) ? 0 : (200 - change);
        green = red;
        blue = (200 + change > 255) ? 255 : (200 + change);
    }

    prevTemp = newTemp;
    glcd.setColor({'red' : red, 'green' : green, 'blue' : blue});
}

function readTemperature() {
    var T = bmp280.readTemperature();
    var whole = T | 0;
    var dec = (T * 10) | 0;
    dec = dec - whole * 10;
    var tempStr = whole  + '.' + dec + "C";

    // Draw icon for direction temp has changed
    if (prevTemp < T) {
        glcd.writeText("+", 0, 1);
    } else if (prevTemp > T) {
        glcd.writeText("-", 0, 1);
    }

    // Write out the temp to the LCD
    glcd.writeText(tempStr, 9, 1);

    // Change the color based on new temp
    setColorFromTemperature(T);
    prevTemp = T;
}

/* Loop read forever */

var timer = setInterval(readTemperature, 10);
