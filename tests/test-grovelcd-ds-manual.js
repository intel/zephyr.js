// Copyright (c) 2016-2017, Intel Corporation.

console.log("test GroveLCD display");

var grove_lcd = require("grove_lcd");

var glcd = grove_lcd.init();

var funs = [
    grove_lcd.GLCD_FS_ROWS_1,
    grove_lcd.GLCD_FS_ROWS_2,
    grove_lcd.GLCD_FS_8BIT_MODE,
    grove_lcd.GLCD_FS_DOT_SIZE_BIG,
    grove_lcd.GLCD_FS_DOT_SIZE_LITTLE
];

var index = [
    [0, 2, "ROW1, 8BIT"],
    [1, 2, "ROW2, 8BIT"],
    [2, 3, "8BIT, BIG"],
    [2, 4, "8BIT, LIT"],
    [0, 3, "ROW1, BIG"],
    [0, 4, "ROW1, LIT"],
    [1, 3, "ROW2, BIG"],
    [1, 4, "ROW2, LIT"],
    [0, 2, 3, "ROW1, 8BIT, BIG"],
    [0, 2, 4, "ROW1, 8BIT, LIT"],
    [1, 2, 3, "ROW2, 8BIT, BIG"],
    [1, 2, 4, "ROW2, 8BIT, LIT"]
];

glcd.setColor(50, 50, 50);

var count = 0;
var funcConfig;

var timer = setInterval(function () {
    if (count === index.length) {
        glcd.clear();
        glcd.setCursorPos(6, 1);
        glcd.print("_END_");

        console.log("Testing completed");

        clearInterval(timer);
        return;
    }

    if (index[count].length === 3) {
        funcConfig = funs[index[count][0]]
                   | funs[index[count][1]];
        glcd.setFunction(funcConfig);

        glcd.clear();
        glcd.setCursorPos(0, 0);
        glcd.print(index[count][2]);

        console.log("setFunction(row, mode, dot): expected result "
                    + index[count][2]);
    }

    if (index[count].length === 4) {
        funcConfig = funs[index[count][0]]
                   | funs[index[count][1]]
                   | funs[index[count][2]];
        glcd.setFunction(funcConfig);

        glcd.clear();
        glcd.setCursorPos(0, 0);
        glcd.print(index[count][3]);

        console.log("setFunction(row, mode, dot): expected result "
                    + index[count][3]);
    }

    count++;
}, 5000);
