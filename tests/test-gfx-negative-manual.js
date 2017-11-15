// Copyright (c) 2017, Intel Corporation.

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
var gfxLib = require("gfx");

try {
    var GFX = gfxLib.init(LCD.width, LCD.height, LCD.initScreen, LCD.drawCB);
    GFX.drawPixel(0, 0, [0xF8, 0x00]);
} catch (e) {
    console.log("\n" + e.name + " : " + e.message);
    console.log("init(width, height, screen, draw): ");
    console.log("expected result: Expected a function");
}

GFX = gfxLib.init(LCD.width, LCD.height, LCD.initScreen, LCD.drawCB, LCD);
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
    for (var i = 0; i < 16; i++) {
        x = 4 * i;
        y = 5 * i;
        width = 128 - 8 * i;
        height = 160 - 10 * i;
        GFX.drawRect(x, y, width, height, color, 5);
    }
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
            [0, 0, 160, 160],
            [0, 0, 128, 200],
            [-100, 0, 128, 160],
            [0, -100, 128, 160]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            try {
                GFX.fillRect(valueArray[k][0], valueArray[k][1],
                             valueArray[k][2], valueArray[k][3], WHITE);
            } catch (e) {
                console.log("\n" + e.name + " : " + e.message);
                console.log("fillRect(x, y, width, height, color): ");
                console.log("expected result: throw out error");
            }
        }
    }

    if (screenCount === 2) {
        backGround(BLACK);

        valueArray = [
            [0, 0, 5, "top left corner"],
            [128, 0, 5, "top right corner"],
            [0, 160, 5, "lower left corner"],
            [128, 160, 5, "lower right corner"]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            drawCircular(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], WHITE);

            console.log("\ndrawPixel(x, y, color): doodle and no error");
            console.log("expected result: draw circular in " +
                        valueArray[k][3] + " outside of the screen");
        }
    }

    if (screenCount === 3) {
        backGround(BLACK);

        valueArray = [
            [30, -100, 200, 1],
            [50, 100, 200, 1],
            [70, -100, 300, 1],
            [-50, -50, 300, 1],
            [160, -50, 300, 1]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawVLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], WHITE, valueArray[k][3]);

            console.log("\ndrawVLine(x, y, height, color, size): " +
                        "doodle and no error");
            console.log("expected result: draw vertical line " +
                        "outside of the screen");
        }
    }

    if (screenCount === 4) {
        backGround(BLACK);

        valueArray = [
            [-100, 30, 200, 1],
            [50, 100, 200, 1],
            [-100, 70, 300, 1],
            [-50, -50, 300, 1],
            [-50, 200, 300, 1]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawHLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], WHITE, valueArray[k][3]);

            console.log("\ndrawHLine(x, y, width, color, size): " +
                        "doodle and no error");
            console.log("expected result: draw horizontal line " +
                        "outside of the screen");
        }
    }

    if (screenCount === 5) {
        backGround(BLACK);

        valueArray = [
            [-50, -50, 50, 50],
            [60, 60, 200, 200],
            [-100, -70, 300, 300],
            [-100, 10, 10, 100],
            [50, 300, 300, 100]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawLine(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], WHITE);

            console.log("\ndrawLine(x0, y0, x1, y1, color): " +
                        "doodle and no error");
            console.log("expected result: draw oblique line " +
                        "outside of the screen");
        }
    }

    if (screenCount === 6) {
        backGround(BLACK);

        valueArray = [
            [50, -50, 50, 100],
            [50, 210, 50, 100],
            [-50, 50, 100, 50],
            [178, 50, 100, 50],
            [40, -60, 70, 280],
            [-60, 40, 70, 248],
            [-10, -10, 148, 180]
        ];
        for (var k = 0; k < valueArray.length; k++) {
            GFX.drawRect(valueArray[k][0], valueArray[k][1],
                         valueArray[k][2], valueArray[k][3], WHITE);

            console.log("\ndrawRect(x, y, width, height, color): " +
                        "doodle and no error");
            console.log("expected result: draw hollow rectangle " +
                        "outside of the screen");
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
}, 5000);
