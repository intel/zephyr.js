// Copyright (c) 2016-2017, Intel Corporation.

console.log("Test GroveLCD APIs");

var grove_lcd = require("grove_lcd");
var assert = require("Assert.js");

// Check pins defined and typeof Number
function checkDefined(name) {
    assert(name in grove_lcd && typeof grove_lcd[name] === "number",
          "groveLCD: defined as '" + name + "'");
}

// test GroveLCD device defined
var glcd = grove_lcd.init();
assert(glcd !== null && typeof glcd === "object",
       "groveLCD: GroveLCD device is defined");

// set cursor
glcd.setCursorPos(0, 0);

assert.throws(function () {
    glcd.setCursorPos("value", 0);
}, "cursor: set col as 'value'");

// print text
var printText = "Hello World";
glcd.print(printText);

assert.throws(function () {
    glcd.print(-1024);
}, "print: set text as '-1024'");

// select LCD color as key
var colorKey = [
    "GROVE_RGB_RED",
    "GROVE_RGB_GREEN",
    "GROVE_RGB_BLUE",
    "GROVE_RGB_WHITE"
];

for (var i = 0; i < colorKey.length; i++) {
    checkDefined(colorKey[i]);
}

// set LCD color as num
glcd.setColor(0, 0, 0);

assert.throws(function () {
    glcd.setColor("value", 0, 0);
}, "color: set invalid value as 'value'");

glcd.setColor(255, 255, 255);

// clear display
glcd.clear();

// set and get function with one keyword
var funcOne = [
    [grove_lcd.GLCD_FS_ROWS_1, 0, "GLCD_FS_ROWS_1"],
    [grove_lcd.GLCD_FS_ROWS_2, 8, "GLCD_FS_ROWS_2"],
    [grove_lcd.GLCD_FS_8BIT_MODE, 16, "GLCD_FS_8BIT_MODE"],
    [grove_lcd.GLCD_FS_DOT_SIZE_BIG, 4, "GLCD_FS_DOT_SIZE_BIG"],
    [grove_lcd.GLCD_FS_DOT_SIZE_LITTLE, 0, "GLCD_FS_DOT_SIZE_LITTLE"]
];

for (var i = 0; i < funcOne.length; i++) {
    checkDefined(funcOne[i][2]);

    glcd.setFunction(funcOne[i][0]);

    assert(glcd.getFunction() === funcOne[i][1],
           "function: set and get as '" + funcOne[i][2]) + "'";
}

// set and get function with more keyword combination
var funcConfig1 = [
    [0, 0, 0],
    [grove_lcd.GLCD_FS_ROWS_1, 0, "ROW1"],
    [grove_lcd.GLCD_FS_ROWS_2, 8, "ROW2"]
];

var funcConfig2 = [
    [0, 0, 0],
    [grove_lcd.GLCD_FS_8BIT_MODE, 16, "8BIT"]
];

var funcConfig3 = [
    [0, 0, 0],
    [grove_lcd.GLCD_FS_DOT_SIZE_BIG, 4, "BIG"],
    [grove_lcd.GLCD_FS_DOT_SIZE_LITTLE, 0, "LIT"]
];

var funcConfig, funcValue, funcLog;
for (var i = 0; i < funcConfig1.length; i++) {
    for (var j = 0; j < funcConfig2.length; j++) {
        for (var k = 0; k < funcConfig3.length; k++) {
            funcConfig = funcConfig1[i][0]
                       | funcConfig2[j][0]
                       | funcConfig3[k][0];

            funcValue = funcConfig1[i][1]
                      + funcConfig2[j][1]
                      + funcConfig3[k][1];

            funcLog = funcConfig1[i][2] + " and "
                    + funcConfig2[j][2] + " and "
                    + funcConfig3[k][2];

            glcd.setFunction(funcConfig);

            if (i === 0 && j === 0 && k === 0) continue;
            if (i === 0 && j === 0) continue;
            if (i === 0 && k === 0) continue;
            if (j === 0 && k === 0) continue;

            if (i !== 0) {
                if (j !== 0) {
                    if (k === 0) {
                        funcLog = funcConfig1[i][2] + " and "
                                + funcConfig2[j][2];
                    }
                } else {
                    if (k !== 0) {
                        funcLog = funcConfig1[i][2] + " and "
                                + funcConfig3[k][2];
                    }
                }
            } else {
                if (j !== 0 && k !== 0) {
                    funcLog = funcConfig2[j][2] + " and "
                            + funcConfig3[k][2];
                }
            }

            assert(glcd.getFunction() === funcValue,
                   "function: keyword as " + funcLog);
        }
    }
}

// set and get display state with one keyword
var displayOne = [
    [grove_lcd.GLCD_DS_DISPLAY_OFF, 0, "GLCD_DS_DISPLAY_OFF"],
    [grove_lcd.GLCD_DS_DISPLAY_ON, 4, "GLCD_DS_DISPLAY_ON"],
    [grove_lcd.GLCD_DS_CURSOR_OFF, 0, "GLCD_DS_CURSOR_OFF"],
    [grove_lcd.GLCD_DS_CURSOR_ON, 2, "GLCD_DS_CURSOR_ON"],
    [grove_lcd.GLCD_DS_BLINK_OFF, 0, "GLCD_DS_BLINK_OFF"],
    [grove_lcd.GLCD_DS_BLINK_ON, 1, "GLCD_DS_BLINK_ON"]
];

for (var i = 0; i < displayOne.length; i++) {
    checkDefined(displayOne[i][2]);

    glcd.setDisplayState(displayOne[i][0]);

    assert(glcd.getDisplayState() === displayOne[i][1],
           "display: set and get as '" + displayOne[i][2] + "'");
}

// set and get cursor 'on' and 'off'
var displayTwo = grove_lcd.GLCD_DS_DISPLAY_ON | grove_lcd.GLCD_DS_CURSOR_ON;
glcd.setDisplayState(displayTwo);

assert(glcd.getDisplayState() === 6,
       "display: set and get with cursor 'on'");

displayTwo = grove_lcd.GLCD_DS_DISPLAY_ON | grove_lcd.GLCD_DS_CURSOR_OFF;
glcd.setDisplayState(displayTwo);

assert(glcd.getDisplayState() === 4,
       "display: set and get with cursor 'off'");

// set and get blink 'on' and 'off'
var displayThree = grove_lcd.GLCD_DS_DISPLAY_ON
                 | grove_lcd.GLCD_DS_CURSOR_ON
                 | grove_lcd.GLCD_DS_BLINK_ON;
glcd.setDisplayState(displayThree);

assert(glcd.getDisplayState() === 7,
       "display: set and get with blink 'on'");

displayThree = grove_lcd.GLCD_DS_DISPLAY_ON
             | grove_lcd.GLCD_DS_CURSOR_ON
             | grove_lcd.GLCD_DS_BLINK_OFF;
glcd.setDisplayState(displayThree);

assert(glcd.getDisplayState() === 6,
       "display: set and get with blink 'off'");
