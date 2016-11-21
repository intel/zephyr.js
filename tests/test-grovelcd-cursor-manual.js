// Copyright (c) 2016, Intel Corporation.

console.log("test cursor position");

var grove_lcd = require("grove_lcd");

var glcd = grove_lcd.init();

var displays = [
    grove_lcd.GLCD_DS_DISPLAY_ON,
    grove_lcd.GLCD_DS_DISPLAY_OFF,
    grove_lcd.GLCD_DS_CURSOR_ON,
    grove_lcd.GLCD_DS_CURSOR_OFF,
    grove_lcd.GLCD_DS_BLINK_ON,
    grove_lcd.GLCD_DS_BLINK_OFF
];

var cursorFlag = true;
var cursorCol = -1;
var cursorRow = 0;
var cursorCount = 0;

glcd.setCursorPos(0, 0);
glcd.setDisplayState(displays[0] | displays[2]);
glcd.print("cursor  position");

var cycleTimer = setInterval(function () {
    if (cursorCount === 0) {
        glcd.setCursorPos(6, 1);
        glcd.print("BLINK");
        glcd.setDisplayState(displays[0] | displays[2] | displays[4]);

        console.log("set cursor as 'on' and blink");
    }

    if (cursorCount === 64) {
        glcd.clear();
        glcd.setCursorPos(6, 1);
        glcd.print("_END_");
        glcd.setDisplayState(displays[0] | displays[3]);

        console.log("Testing completed");

        clearInterval(cycleTimer);
    }

    if (cursorFlag) {
        cursorCol++;
    } else {
        cursorCol--;
    }

    if (cursorCount <= 31) {
        if (cursorCol === 16) {
            cursorFlag = false;
            cursorCol = 15;
            cursorRow = 1;
        }
    }

    if (cursorCount === 32) {
        cursorFlag = true;
        glcd.setCursorPos(5, 1);
        glcd.print("NOBLINK");
        cursorCol = 0;
        cursorRow = 1;
        glcd.setDisplayState(displays[0] | displays[2]);

        console.log("set cursor as 'on' and no blink");
    }

    if (32 <= cursorCount && cursorCount <= 63) {
        if (cursorCol === 16) {
            cursorFlag = false;
            cursorCol = 15;
            cursorRow = 0;
        }
    }

    if (cursorCount <= 63) {
        glcd.setCursorPos(cursorCol, cursorRow);

        console.log("setCursorPos(col, row): expected result "
                    + cursorCol + ", " + cursorRow);
    }

    cursorCount++;
}, 1000);
