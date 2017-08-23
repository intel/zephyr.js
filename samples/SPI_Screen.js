// Copyright (c) 2017, Intel Corporation.
// JS for using the SPI LCD screen ST7735 module
// Setup:
// DC - Pin 8
// CS - Pin 4
// RST - Pin 7
// SCL - Pin 13
// SDA - Pin 11

// Color definitions
var BLACK =  [0x00,0x00];
var BLUE  =  [0x00,0x1F];
var RED   =  [0xF8,0x00];
var GREEN =  [0x07,0xE0];
var CYAN  =  [0x07,0xFF];
var MAGENTA = [0xF8,0x1F];
var YELLOW =  [0xFF,0xE0];
var WHITE =  [0xFF,0xFF];

var LCD = require("ST7735.js");
var gpio = require('gpio');

console.log("SPI screen test starting..");

try {
    LCD.initScreen();
    LCD.fillRect(0, 0, LCD.width, LCD.height, BLACK);
    LCD.drawLine(30, 70, 40, 100, YELLOW);
    LCD.drawLine(70, 30, 100, 50, CYAN);
    LCD.fillRect(50, 50, 100, 100, RED);
    LCD.drawPixel(64, 80, GREEN);
    LCD.drawPixel(42, 53, CYAN);
    LCD.drawPixel(32, 40, MAGENTA);
    stopJS();
} catch (err) {
  console.log("SPI error: " + err.message);
}
