// Copyright (c) 2016, Intel Corporation.

console.log("test RGB color");

var grove_lcd = require("grove_lcd");

var glcd = grove_lcd.init();

var colorNum = [
    [255, 0, 0],
    [0, 255, 0],
    [0, 0, 255],
    [255, 255, 0],
    [255, 0, 255],
    [0, 255, 255],
    [255, 255, 255]
];

var colorName = [
    " red  ",
    "green ",
    " blue ",
    "yellow",
    "purple",
    " cyan ",
    "white "
];

var displayConfig = grove_lcd.GLCD_DS_DISPLAY_ON
                  | grove_lcd.GLCD_DS_CURSOR_OFF;
glcd.setDisplayState(displayConfig);

glcd.setCursorPos(1, 0);
glcd.print("Color Gradient");

var flag = true;
var redFlag = colorNum[0][0];
var greenFlag = colorNum[0][1];
var blueFlag = colorNum[0][2];
var redNum = 0;
var greenNum = 0;
var blueNum = 0;
var colorCount = 0;
var count = 0;

var colorGradient = setInterval(function () {
    glcd.setCursorPos(5, 1);
    glcd.print(colorName[colorCount]);

    if (count === 102) {
        colorCount++;
        count = 0;

        if (colorCount === 7) {
            glcd.clear();
            glcd.setCursorPos(6, 1);
            glcd.print("_END_");

            console.log("Testing completed");

            clearInterval(colorGradient);
            return;
        }

        redFlag = colorNum[colorCount][0];
        greenFlag = colorNum[colorCount][1];
        blueFlag = colorNum[colorCount][2];
    }

    if (count === 0) {
        console.log("setColor(R, G, B): expected result "
                    + redFlag + ", " + greenFlag + ", " + blueFlag);
        console.log("color gradient: " + colorName[colorCount]);
    }

    if (redNum === 255 || greenNum === 255 || blueNum === 255) {
        flag = false;
    }

    if (flag) {
        if (redFlag > 0) {
            redNum += 5;
        }

        if (greenFlag > 0) {
            greenNum += 5;
        }

        if (blueFlag > 0) {
            blueNum += 5;
        }
    } else {
        if (redFlag > 0) {
            redNum -= 5;

            if (redNum === 0) {
                flag = true;
            }
        }

        if (greenFlag > 0) {
            greenNum -= 5;

            if (greenNum === 0) {
                flag = true;
            }
        }

        if (blueFlag > 0) {
            blueNum -= 5;

            if (blueNum === 0) {
                flag = true;
            }
        }
    }

    glcd.setColor(redNum, greenNum, blueNum);
    count++;
}, 100);
