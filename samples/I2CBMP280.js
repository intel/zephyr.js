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

function bmp280Init() {
    var reader = bmp280.i2cDevice.read(bmp280.bmp280Addrs.ADDRESS, 1,
                                       bmp280.bmp280Addrs.REGISTER_CHIPID);

    // Check that we have a BMP280 on the I2C bus
    if (reader.toString("hex") !== "58")
        print("BMP280 not found! ChipId " + reader.toString("hex") !== "58");

    // Setup BMP280 sensor
    bmp280.readCoefficients();

    bmp280.i2cDevice.write(bmp280.bmp280Addrs.ADDRESS,
                           new Buffer([bmp280.bmp280Addrs.REGISTER_CONTROL,
                           bmp280.bmp280Addrs.PRESS_OVER |
                           bmp280.bmp280Addrs.TEMP_OVER |
                           bmp280.bmp280Addrs.NORMAL_MODE]));

    bmp280.i2cDevice.write(bmp280.bmp280Addrs.ADDRESS,
                           new Buffer([bmp280.bmp280Addrs.REGISTER_CONFIG,
                           bmp280.bmp280Addrs.STANDBY |
                           bmp280.bmp280Addrs.FILTER |
                           bmp280.bmp280Addrs.SPI_3W_DISABLE]));
}

function glcdInit() {

    // Clear the screen
    glcd.clear();

    // Set our preferences for the Grove LCD
    glcd.i2cDevice.write(glcd.glcdAddrs.DISPLAY_ADDR,
                         new Buffer([0, glcd.glcdAddrs.CMD_FUNCTION_SET |
                         glcd.glcdAddrs.FS_ROWS_2 |
                         glcd.glcdAddrs.FS_DOT_SIZE_LITTLE |
                         glcd.glcdAddrs.FS_8BIT_MODE]));

    glcd.i2cDevice.write(glcd.glcdAddrs.DISPLAY_ADDR,
                         new Buffer([0, glcd.glcdAddrs.CMD_DISPLAY_SWITCH |
                         glcd.glcdAddrs.DS_DISPLAY_ON |
                         glcd.glcdAddrs.DS_CURSOR_OFF |
                         glcd.glcdAddrs.DS_BLINK_OFF]));

    // Init backlight
    glcd.i2cDevice.write(glcd.glcdAddrs.BACKLIGHT_ADDR,
                         new Buffer([glcd.glcdAddrs.REG_MODE1, 0x00]));

    glcd.i2cDevice.write(glcd.glcdAddrs.BACKLIGHT_ADDR,
                         new Buffer([glcd.glcdAddrs.REG_MODE2, 0x05]));

    glcd.i2cDevice.write(glcd.glcdAddrs.BACKLIGHT_ADDR,
                         new Buffer([glcd.glcdAddrs.REGISTER_POWER, 0xFF]));

    // Init the screen with text
    glcd.writeText("Temperature is ", 0, 0);
}

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
    glcd.setColor({"red" : red, "green" : green, "blue" : blue});
}

function readTemperature() {
    var T = bmp280.readTemperature();
    var whole = T | 0;
    var dec = (T * 10) | 0;
    dec = dec - whole * 10;
    var tempStr = whole  + "." + dec + "C";

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

bmp280Init();
glcdInit();

/* Loop read forever */

var timer = setInterval(readTemperature, 10);
