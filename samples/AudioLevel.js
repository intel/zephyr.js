// Copyright (c) 2017, Intel Corporation.

// Sample code for Arduino 101 w/ Grove LCD and Grove Sound Sensor (or Rotary
// Angle Sensor) on A1. Make noise or turn the potentiometer to simluate it,
// and see a visual sound level meter on the LCD.

console.log("Audio Level sample...");

var pins = require("arduino101_pins");
var aio = require("aio");
var lcd = require("grove_lcd");

// set initial state
var funcConfig = lcd.GLCD_FS_ROWS_2
               | lcd.GLCD_FS_DOT_SIZE_LITTLE
               | lcd.GLCD_FS_8BIT_MODE;

var glcd = lcd.init();
glcd.setFunction(funcConfig);
glcd.setDisplayState(lcd.GLCD_DS_DISPLAY_ON);

glcd.clear();
glcd.setColor(0, 32, 0);

glcd.setCursorPos(0, 0);
glcd.print('|');
glcd.setCursorPos(0, 1);
glcd.print('|');

var lastlevel = 0;
var lastbright = 0;

var knob = aio.open({ device: 0, pin: pins.A1 });
setInterval(function () {
    var value = 4096.0 - knob.read();
    var level = (value / 256.0) | 0;

    if (level <= 12) {
        var brightness = value / 18.4 + 32;
        if (lastbright === brightness)
            return;
        lastbright = brightness;
        glcd.setColor(0, brightness, 0);
    }

    if (level === lastlevel)
        return;

    // change to red
    if (level > 12 && lastlevel <= 12)
        glcd.setColor(255, 0, 0);

    var min = lastlevel;
    var max = level;
    if (level < lastlevel) {
        min = level;
        max = lastlevel;
    }

    var line = "";
    for (var i = min; i <= max; i++) {
        if (i < level) {
            line += '=';
        }
        else if (i === level) {
            line += '|';
        }
        else {
            line += ' ';
        }
    }

    glcd.setCursorPos(min, 0);
    glcd.print(line);
    glcd.setCursorPos(min, 1);
    glcd.print(line);

    lastlevel = level;
}, 100);
