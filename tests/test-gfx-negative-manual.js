// Copyright (c) 2017-2018, Intel Corporation.

// Software Requirements:
//     ST7735.js module
// Hardware Requirements:
//     A SPI LCD screen(ST7735)
// Wiring:
//     For SPI LCD screen(ST7735):
//         Wire VCC on SPI LCD screen(ST7735) to Vcc(3.3v) on the Arduino 101
//         Wire GND on SPI LCD screen(ST7735) to GND on the Arduino 101
//         Wire LED+ on SPI LCD screen(ST7735) to Vcc(3.3v) on the Arduino 101
//         Wire LED- on SPI LCD screen(ST7735) to GND on the Arduino 101
//         Wire CS on SPI LCD screen(ST7735) to IO4 on the Arduino 101
//         Wire RST on SPI LCD screen(ST7735) to IO7 on the Arduino 101
//         Wire DC(AO) on SPI LCD screen(ST7735) to IO8 on the Arduino 101
//         Wire SDA on SPI LCD screen(ST7735) to IO11 on the Arduino 101
//         Wire SCL on SPI LCD screen(ST7735) to IO13 on the Arduino 101

console.log("Test GFX APIs with SPI LCD screen(ST7735)");

var LCD = require("ST7735.js");
var board = require("board");
var drawImmediate = board.name === "arduino_101" ? true : false;
var gfxLib = require("gfx");

var GFX = gfxLib.init(LCD.width, LCD.height, LCD.initScreen,
                      LCD.drawCB, drawImmediate);
GFX.drawPixel(0, 0, [0xF8, 0x00]);

console.log("\ninit(width, height, screen, draw): ");
console.log("expected result: throw out error");

GFX = gfxLib.init(LCD.width, LCD.height, LCD.initScreen,
                  LCD.drawCB, drawImmediate, LCD);
console.log("\ninit(width, height, screen, draw, optional): ");
console.log("expected result: screen init successful");

// Color init
var BLACK =  [0x00, 0x00];
var BLUE  =  [0x00, 0x1F];
var RED   =  [0xF8, 0x00];
var GREEN =  [0x07, 0xE0];
var CYAN  =  [0x07, 0xFF];
var MAGENTA = [0xF8, 0x1F];
var YELLOW =  [0xFF, 0xE0];
var WHITE =  [0xFF, 0xFF];

var x = 0;
var y = 0;
var width = 0;
var height = 0;
var radius = 0;

var backGround = function(color) {
    GFX.fillRect(0, 0, 128, 160, color);
}

var drawCircular = function(x, y, radius, color) {
    for (var i = -radius; i <= radius; i++) {
        for (var j = -radius; j <= radius; j++) {
            if ((i * i + j * j) <= radius * radius) {
                GFX.drawPixel(x + i, y + j, color);
            }
        }
    }
}

var valueArray;
var screenCount = 0;
var screenTimer = setInterval(function () {
    screenCount++;

    if (screenCount === 1) {
        backGround(BLACK);

        valueArray = [
            [0, 0, 160, 160, BLUE, "full screen with blue color"],
            [0, 0, 128, 200, RED, "full screen with red color"],
            [-100, 0, 228, 160, GREEN, "full screen with green color"],
            [0, -100, 128, 260, CYAN, "full screen with cyan color"],
            [-100, 40, 328, 80, MAGENTA, "half screen with magenta color"],
            [32, -100, 64, 360, YELLOW, "half screen with yellow color"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.fillRect(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], valueArray[k][4]);

            console.log("\nfillRect(x, y, width, height, color):");
            console.log("expected result: draw a fill rectangle with " +
                        valueArray[k][5]);
        }
    }

    if (screenCount === 2) {
        backGround(BLACK);

        valueArray = [
            [0, 0, 5, "top left corner"],
            [127, 0, 5, "top right corner"],
            [0, 159, 5, "lower left corner"],
            [127, 159, 5, "lower right corner"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            drawCircular(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], WHITE);

            console.log("\ndrawPixel(x, y, color):");
            console.log("expected result: draw circular in " +
                        valueArray[k][3] + " of the screen");
        }
    }

    if (screenCount === 3) {
        backGround(BLACK);

        valueArray = [
            [32, -50, 100, BLUE, "on top of the screen with blue color"],
            [64, 50, 200, RED, "on lower of the screen with red color"],
            [96, -50, 260, GREEN, "across the screen with green color"],
            [-50, -50, 260, CYAN, "on outside of the screen with cyan color"],
            [160, -50, 260, MAGENTA, "on outside of the screen with magenta color"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawVLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], 1);

            console.log("\ndrawVLine(x, y, height, color, size):");
            console.log("expected result: draw vertical line " +
                        valueArray[k][4]);
        }
    }

    if (screenCount === 4) {
        backGround(BLACK);

        valueArray = [
            [-50, 40, 100, BLUE, "on left of the screen with blue color"],
            [50, 80, 200, RED, "on right of the screen with red color"],
            [-50, 120, 228, GREEN, "across the screen with green color"],
            [-50, -50, 228, CYAN, "on outside of the screen with cyan color"],
            [-50, 200, 228, MAGENTA, "on outside of the screen with magenta color"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawHLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], 1);

            console.log("\ndrawHLine(x, y, width, color, size):");
            console.log("expected result: draw horizontal line " +
                        valueArray[k][4]);
        }
    }

    if (screenCount === 5) {
        backGround(BLACK);

        valueArray = [
            [-50, -50, 50, 50, BLUE, "on top left corner of the screen with blue color"],
            [78, 110, 178, 210, RED, "on lower right corner of the screen with red color"],
            [-50, 210, 178, -50, GREEN, "across the screen with green color"],
            [-50, -50, -100, -100, CYAN, "on outside of the screen with cyan color"],
            [200, 200, 300, 300, MAGENTA, "on outside of the screen with magenta color"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], valueArray[k][4]);

            console.log("\ndrawLine(x0, y0, x1, y1, color):");
            console.log("expected result: draw oblique line " +
                        valueArray[k][5]);
        }
    }

    if (screenCount === 6) {
        backGround(BLACK);

        valueArray = [
            [40, -10, 48, 50, BLUE, "on top of the screen with blue color"],
            [40, 120, 48, 50, RED, "on lower of the screen with red color"],
            [-10, 40, 50, 80, GREEN, "on left of the screen with green color"],
            [88, 40, 50, 80, CYAN, "on right of the screen with cyan color"],
            [-10, 20, 148, 120, MAGENTA, "across of the screen with magenta color"],
            [20, -10, 88, 180, YELLOW, "across of the screen with yellow color"],
            [-10, -10, 148, 180, WHITE, "on outside of the screen with white color"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawRect(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], valueArray[k][4]);

            console.log("\ndrawRect(x, y, width, height, color):");
            console.log("expected result: draw hollow rectangle " +
                        valueArray[k][5]);
        }
    }

    if (screenCount === 7) {
        var charCount = 0;
        var charTimer = setInterval(function () {
            if (charCount < 15) {
                backGround(BLACK);
                GFX.drawChar(0, 0, "I", WHITE, charCount + 1);
            } else {
                console.log("\ndrawChar(x, y, char, color, size): ");
                console.log("expected result: gradually enlarged");

                clearInterval(charTimer);
            }

            charCount++;
        }, 1000);
    }

    if (screenCount === 11) {
        var stringCount = 0;
        var stringTimer = setInterval(function () {
            if (stringCount < 15) {
                backGround(BLACK);
                GFX.drawString(0, 0, "ZJS", WHITE, stringCount + 1);
            } else {
                console.log("\ndrawString(x, y, str, color, size): ");
                console.log("expected result: gradually enlarged");

                clearInterval(stringTimer);
            }

            stringCount++;
        }, 1000);
    }

    if (screenCount === 15) {
        console.log("\nTesting completed");

        backGround(BLACK);
        clearInterval(screenTimer);
    }
}, 7000);
