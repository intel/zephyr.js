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
                      LCD.drawCB, drawImmediate, LCD);

// Color init
var BLACK =  [0x00, 0x00];
var BLUE  =  [0x00, 0x1F];
var RED   =  [0xF8, 0x00];
var GREEN =  [0x07, 0xE0];
var CYAN  =  [0x07, 0xFF];
var MAGENTA = [0xF8, 0x1F];
var YELLOW =  [0xFF, 0xE0];
var WHITE =  [0xFF, 0xFF];
var ColorArray = [
    [BLUE, "blue"],
    [RED, "red"],
    [YELLOW, "yellow"],
    [CYAN, "cyan"],
    [MAGENTA, "magenta"],
    [GREEN, "green"]
];

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

var chessPiece = function(x, y, radius, color) {
    for (var i = -radius; i <= radius; i++) {
        for (var j = -radius; j <= radius; j++) {
            if ((i * i + j * j) <= radius * radius) {
                GFX.drawPixel(x * 8 + i, y * 10 + j, color);
            }
        }
    }

    console.log("\ndrawPixel(x, y, color): ");
    console.log("expected result: draw circular on [" + (x * 8) + ", " +
                (y * 10) + "] point with " + radius + " radius");
}

var ChessPieceMap = [
    [8, 8],
    [9, 7],
    [9, 9],
    [7, 7],
    [10, 8],
    [11, 7],
    [9, 8],
    [10, 7],
    [8, 7],
    [11, 8],
    [7, 8],
    [6, 8],
    [8, 9],
    [8, 6],
    [10, 9]
];

var screenCount = 0;
var screenTimer = setInterval(function () {
    screenCount++;

    if (screenCount === 1) {
        // test fillRect API
        backGround(BLACK);

        var FillRectArray = [
            [0, 0, 0, 0, "on top left corner"],
            [86, 0, 6, 0, "on top right corner"],
            [0, 118, 0, 6, "on lower left corner"],
            [86, 118, 6, 6, "on lower right corner"],
            [43, 59, 3, 3, "in the middle"]
        ];
        for (var i = 0; i < 5; i++) {
            for (var j = 0; j < 6; j++) {
                x = FillRectArray[i][0] + (FillRectArray[i][2] * j);
                y = FillRectArray[i][1] + (FillRectArray[i][3] * j);
                GFX.fillRect(x, y, 42 - (6 * j), 42 - (6 * j), ColorArray[j][0]);

                console.log("\nfillRect(x, y, width, height, color): ");
                console.log("expected result: draw " + (42 - 6 * j) + "*" +
                            (42 - 6 * j) + " solid rectangle with " +
                            ColorArray[j][1] + " color " + FillRectArray[i][4]);
            }

            console.log("expected result: rectangular area gradually narrow " +
                        FillRectArray[i][4]);
        }
    }

    if (screenCount === 2) {
        // test drawRect API
        for (var i = 0; i < 64; i++) {
            x = 1 * i;
            y = 1 * i;
            width = 128 - 2 * i;
            height = 160 - 2 * i;
            GFX.drawRect(x, y, width, height, BLACK);

            if (i === 0) {
                console.log("\ndrawRect(x, y, width, height, color): ");
                console.log("expected result: draw 1 * 1 hollow rectangle " +
                            "with 1 edge width");
            }

            if (i === 63) {
                console.log("\ndrawRect(x, y, width, height, color): ");
                console.log("expected result: draw 128 * 160 hollow rectangle " +
                            "with 1 edge width");
            }
        }

        console.log("expected result: rectangle size gradually narrow " +
                    "with [0x00, 0x00] color");

        for (var i = 0; i < 16; i++) {
            x = 60 - 4 * i;
            y = 75 - 5 * i;
            width = 8 + 8 * i;
            height = 10 + 10 * i;
            GFX.drawRect(x, y, width, height, [0xF4, 0xA0], 5);

            if (i === 0) {
                console.log("\ndrawRect(x, y, width, height, color, size): ");
                console.log("expected result: draw 8 * 10 hollow rectangle " +
                            "with 5 edge width");
            }

            if (i === 15) {
                console.log("\ndrawRect(x, y, width, height, color, size): ");
                console.log("expected result: draw 128 * 160 hollow rectangle" +
                           "with 5 edge width");
            }
        }

        console.log("expected result: rectangle size gradually enlarged " +
                    "with [0xF4, 0xA0] color");
    }

    if (screenCount === 3) {
        // test drawVLine API
        for (var i = 8; i < 128; i = i + 8) {
            GFX.drawVLine(i, 0, 160, GREEN);
        }

        console.log("\ndrawVLine(x, y, height, color)");
        console.log("expected result: draw 16 the same interval " +
                    "vertical lines with 1 line width");

        // test drawHLine API
        for (var i = 10; i < 160; i = i + 10) {
            GFX.drawHLine(0, i, 128, GREEN);
        }

        console.log("\ndrawHLine(x, y, width, color)");
        console.log("expected result: draw 16 the same interval " +
                    "horizontal lines with 1 line width");

        // test drawString API
        var NumberArray = [
            "1", "2", "3", "4", "5",
            "6", "7", "8", "9", "10",
            "11", "12", "13", "14", "15"
        ];
        for (var i = 0; i < NumberArray.length; i++) {
            GFX.drawString(3, 10 * i, NumberArray[i], BLUE);
        }

        console.log("\ndrawString(x, y, str, color): ");
        console.log("expected result: draw the same interval " +
                    "horizontal string with 1 ~ 15 number");

        for (var i = 0; i < NumberArray.length; i++) {
            GFX.drawString(8 * i + 3, 0, NumberArray[i], BLUE);
        }
        console.log("\ndrawString(x, y, str, color): ");
        console.log("expected result: draw the same interval " +
                    "vertical string with 1 ~ 15 number");

        GFX.drawVLine(0, 0, 160, RED, 2);
        console.log("\ndrawVLine(x, y, height, color, size)");
        console.log("expected result: draw a vertical line " +
                    "on [0, 0] point with 2 line width");

        GFX.drawVLine(126, 0, 160, RED, 2);
        console.log("\ndrawVLine(x, y, height, color, size)");
        console.log("expected result: draw a vertical line " +
                    "on [128, 0] point with 2 line width");

        GFX.drawHLine(0, 0, 128, RED, 2);
        console.log("\ndrawHLine(x, y, width, color, size)");
        console.log("expected result: draw a horizontal line" +
                   "on [0, 0] point with 2 line width");

        GFX.drawHLine(0, 158, 128, RED, 2);
        console.log("\ndrawHLine(x, y, width, color, size)");
        console.log("expected result: draw a horizontal line" +
                   "on [0, 160] point with 2 line width");

        console.log("expected result: Gobang chess board");
    }

    if (screenCount === 4) {
        // test drawPixel API
        var ChessPieceCount = 0;
        var ChessPieceColor = BLACK;
        var ChessPieceTimer = setInterval(function () {
            if (ChessPieceCount < ChessPieceMap.length) {
                x = ChessPieceMap[ChessPieceCount][0];
                y = ChessPieceMap[ChessPieceCount][1];
                radius = 3;
                chessPiece(x, y, radius, ChessPieceColor);
            } else {
                chessPiece(0, 0, 2, BLUE);
                chessPiece(0, 16, 2, MAGENTA);
                chessPiece(16, 0, 2, YELLOW);
                chessPiece(16, 16, 2, CYAN);
                chessPiece(5, 5, 2, RED);
                chessPiece(10, 10, 2, GREEN);

                console.log("expected result: Gobang chess pieces");

                clearInterval(ChessPieceTimer);
            }

            if (ChessPieceColor === WHITE) {
                ChessPieceColor = BLACK;
            } else {
                ChessPieceColor = WHITE;
            }

            ChessPieceCount++;
        }, 1000);
    }

    if (screenCount === 8) {
        // test drawLine API
        GFX.drawLine(48, 50, 96, 100, RED);
        console.log("\ndrawLine(x0, y0, x1, y1, color): ");
        console.log("expected result: oblique line on " +
                    "[[48, 50], [96, 100]] with 1 line width");

        GFX.drawLine(48, 90, 96, 90, RED, 2);
        console.log("\ndrawLine(x0, y0, x1, y1, color, size): ");
        console.log("expected result: horizontal line on " +
                    "[[48, 90], [96, 90]] with 2 line width");
    }

    if (screenCount === 9) {
        // test drawChar API
        var charMap = [
            [0, "W"],
            [40, "i"],
            [48, "n"],
            [69, "n"],
            [90, "e"],
            [111, "r"]
        ];
        for (var i = 0; i < charMap.length; i++) {
            GFX.drawChar(charMap[i][0], 10, charMap[i][1], ColorArray[i][0], 4);

            console.log("\ndrawChar(x, y, char, color, size): ");
            console.log("expected result: draw char '" + charMap[i][1] +
                        "' with " + ColorArray[i][1] + " color");
        }

        // test drawString API
        GFX.drawString(60, 65, "is", BLUE, 2);
        console.log("\ndrawString(x, y, str, color, size): ");
        console.log("expected result: draw string 'is' with blue color");

        var stringCount = 0;
        var stringTimer = setInterval(function () {
            if (stringCount < ColorArray.length) {
                GFX.drawString(19, 80, "WHITE", ColorArray[stringCount][0], 3);
                console.log("\ndrawString(x, y, str, color, size): ");
                console.log("expected result: draw string 'WHITE' with " +
                            ColorArray[stringCount][1] + " color");
            } else {
                clearInterval(stringTimer);
            }

            stringCount++;
        }, 100);
    }

    if (screenCount === 10) {
        backGround(BLACK);

        var lineMap = [
            [0, 0, 128, 0],
            [126, 0, 126, 160],
            [0, 157, 128, 157],
            [0, 0, 0, 160],
            [0, 0, 128, 160],
            [128, 0, 0, 160]
        ];
        for (var i = 0; i < lineMap.length; i++) {
            GFX.drawLine(lineMap[i][0], lineMap[i][1], lineMap[i][2],
                         lineMap[i][3], ColorArray[i][0], i + 1);

            console.log("\ndrawLine(x0, y0, x1, y1, color, size): ");
            console.log("expected result: draw " + ColorArray[i][1] +
                        " line on " + "[[" + lineMap[i][0] + ", " +
                        lineMap[i][1] + "], [" + lineMap[i][2] + ", " +
                        lineMap[i][3] + "]] with " + (i + 1) + " line width");
        }
    }

    if (screenCount === 11) {
        console.log("\nTesting completed");

        backGround(BLACK);
        clearInterval(screenTimer);
    }
}, 5000);
