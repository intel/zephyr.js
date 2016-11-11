// Copyright (c) 2016, Intel Corporation.
// JavaScript library for the Grove LCD

function GroveLCD() {

    // API object
    var groveLCDAPI = {};

    var i2c = require("i2c");
    groveLCDAPI.i2cDevice = i2c.open({ bus: 0, speed: 100 });

    var glcd = {
        DISPLAY_ADDR : 0x3E,
        BACKLIGHT_ADDR : 0x62,
        // Defines for the CMD_CURSOR_SHIFT
        CS_DISPLAY_SHIFT : 1 << 3,
        CS_RIGHT_SHIFT : 1 << 2,
        // LCD Display Commands
        CMD_SCREEN_CLEAR : 1 << 0,
        CMD_CURSOR_RETURN : 1 << 1,
        CMD_INPUT_SET : 1 << 2,
        CMD_DISPLAY_SWITCH : 1 << 3,
        CMD_CURSOR_SHIFT : 1 << 4,
        CMD_FUNCTION_SET : 1 << 5,
        CMD_SET_CGRAM_ADDR : 1 << 6,
        CMD_SET_DDRAM_ADDR : 1 << 7,
        // Defines for the LCD_FUNCTION_SET
        FS_8BIT_MODE : 1 << 4,
        FS_ROWS_2 : 1 << 3,
        FS_ROWS_1 : 0 << 3,
        FS_DOT_SIZE_BIG : 1 << 2,
        FS_DOT_SIZE_LITTLE : 0 << 2,
        // Defines for the CMD_DISPLAY_SWITCH options
        DS_DISPLAY_ON : 1 << 2,
        DS_DISPLAY_OFF : 0 << 2,
        DS_CURSOR_ON : 1 << 1,
        DS_CURSOR_OFF : 0 << 1,
        DS_BLINK_ON : 1 << 0,
        DS_BLINK_OFF : 0 << 0,
        REGISTER_POWER : 0x08,
        REGISTER_R : 0x04,
        REGISTER_G : 0x03,
        REGISTER_B : 0x02
    };

    groveLCDAPI.clear = function(col, row, numChar) {
        // col = Column you want to start at
        // row = Row you want to start at
        // numChar = Number of chars to clear

        if (numChar !== undefined && numChar < 1) {
            // Can't clear zero or negative numbers of chars
            console.log("GroveLCD.clear numChar must be greater than 0");
            return;
        }

        if (col === undefined && row === undefined && numChar === undefined) {
            // Clear the display completely if no args passed.
            groveLCDAPI.i2cDevice.write(glcd.DISPLAY_ADDR,
                                        new Buffer([0, glcd.CMD_SCREEN_CLEAR]));
            return;
        }
        else if (col === undefined || row === undefined || numChar === undefined) {
            // If any of the args undefined - print warning and return
            console.log ("Missing args for GroveLCD.clear. Please pass col, row, numChar");
            return;
        }

        // Move the cursor
        this.resetCursor(col,row);

        // Create blank space
        var blanks = ' ';

        for (var i = 0; i < numChar - 1; i++) {
            blanks = blanks + ' ';
        }

        // Write blank space to LCD
        this.writeText(blanks);
    }

    groveLCDAPI.resetCursor = function (col, row) {
        // Reset the cursor position
        row = row || 0;
        col = col || 0;

        if (row == 0) {
            // Put the cursor on the first row at col
            col |= 0x80;
        } else {
            // Put the cursor on the second row at col
            col |= 0xC0;
        }

        this.i2cDevice.write(glcd.DISPLAY_ADDR,
                             new Buffer([glcd.CMD_SET_DDRAM_ADDR, col]));
    }

    groveLCDAPI.setColor = function(colorObj) {
        // Valid range for color is 0 - 255
        // Write new color values
        if (colorObj.blue && colorObj.blue > -1 && colorObj.blue < 256) {
            this.i2cDevice.write(glcd.BACKLIGHT_ADDR,
                                 new Buffer([glcd.REGISTER_B, blue]));
        }
        if (colorObj.red && colorObj.red > -1 && colorObj.red < 256) {
            this.i2cDevice.write(glcd.BACKLIGHT_ADDR,
                                 new Buffer([glcd.REGISTER_R, red]));
        }
        if (colorObj.green && colorObj.green > -1 && colorObj.green < 256) {
            this.i2cDevice.write(glcd.BACKLIGHT_ADDR,
                                 new Buffer([glcd.REGISTER_G, green]));
        }
    }


    groveLCDAPI.writeText = function(word, col, row) {

        if (col !== undefined && row !== undefined) {
            // Move cursor to the specified location first
            this.resetCursor(col, row);
        }

        var wordBuffer = new Buffer(word.length + 1);

        // The first byte in the buffer is for the register address,
        // which is where the word is going to be written to.
        wordBuffer.writeUInt8(glcd.CMD_SET_CGRAM_ADDR, 0);

        // Append the text we want to print after that
        wordBuffer.write(word, 1);

        // Write the buffer to I2C
        this.i2cDevice.write(glcd.DISPLAY_ADDR, wordBuffer);
    }

    function init() {

        // Clear the screen
        groveLCDAPI.clear();

        // Set our preferences for the Grove LCD
        groveLCDAPI.i2cDevice.write(glcd.DISPLAY_ADDR,
                                    new Buffer([0, glcd.CMD_FUNCTION_SET |
                                    glcd.FS_ROWS_2 |
                                    glcd.FS_DOT_SIZE_LITTLE |
                                    glcd.FS_8BIT_MODE]));

        groveLCDAPI.i2cDevice.write(glcd.DISPLAY_ADDR,
                                    new Buffer([0, glcd.CMD_DISPLAY_SWITCH |
                                    glcd.DS_DISPLAY_ON |
                                    glcd.DS_CURSOR_OFF |
                                    glcd.DS_BLINK_OFF]));

        groveLCDAPI.resetCursor(0, 0);
    }

    init();

    return groveLCDAPI;
};

module.exports.GroveLCD = new GroveLCD();
