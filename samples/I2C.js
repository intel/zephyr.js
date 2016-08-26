// Copyright (c) 2016, Intel Corporation.

// Sample code demonstrating how to use I2C to communicate with slave devices

// Hardware Requirements:
//   - A Grove LCD
//   - pulldown resistors for SDA and SCL
// Wiring:
//   For LCD:
//     - Wire SDA on the LCD above the pulldown resistor. Connect that resistor to power (VCC)
//     - Wire SDL on the LCD above the pulldown resistor. Connect that resistor to ground (GND)
//     - Wire the SDA for the Arduino and place it between the wire for the LCD and the resistor
//     - Do the same for SDL
//     - Wire power and ground accordingly

// import i2c module
var i2c = require("i2c");

var GROVE_LCD_DISPLAY_ADDR = 0x3E
var GROVE_RGB_BACKLIGHT_ADDR = 0x62

// Defines for the GLCD_CMD_CURSOR_SHIFT
var GLCD_CS_DISPLAY_SHIFT = 1 << 3;
var GLCD_CS_RIGHT_SHIFT   = 1 << 2;

// LCD Display Commands
var GLCD_CMD_SCREEN_CLEAR    = 1 << 0;
var GLCD_CMD_CURSOR_RETURN   = 1 << 1;
var GLCD_CMD_INPUT_SET       = 1 << 2;
var GLCD_CMD_DISPLAY_SWITCH  = 1 << 3;
var GLCD_CMD_CURSOR_SHIFT    = 1 << 4;
var GLCD_CMD_FUNCTION_SET    = 1 << 5;
var GLCD_CMD_SET_CGRAM_ADDR  = 1 << 6;
var GLCD_CMD_SET_DDRAM_ADDR  = 1 << 7;

// Defines for the LCD_FUNCTION_SET
var GLCD_FS_8BIT_MODE       = 1 << 4;
var GLCD_FS_ROWS_2          = 1 << 3;
var GLCD_FS_ROWS_1          = 0 << 3;
var GLCD_FS_DOT_SIZE_BIG    = 1 << 2;
var GLCD_FS_DOT_SIZE_LITTLE = 0 << 2;

// Defines for the GLCD_CMD_DISPLAY_SWITCH options
var GLCD_DS_DISPLAY_ON    = 1 << 2;
var GLCD_DS_DISPLAY_OFF   = 0 << 2;
var GLCD_DS_CURSOR_ON     = 1 << 1;
var GLCD_DS_CURSOR_OFF    = 0 << 1;
var GLCD_DS_BLINK_ON      = 1 << 0;
var GLCD_DS_BLINK_OFF     = 0 << 0;

/********************************************
 *  RGB FUNCTIONS
 *******************************************/

var REGISTER_POWER  = 0x08
var REGISTER_R      = 0x04
var REGISTER_G      = 0x03
var REGISTER_B      = 0x02

print("I2C sample...");

var msgData = new Buffer(2);
var hello = true;
var lcdDispAddr = {address: GROVE_LCD_DISPLAY_ADDR, length: 2};
var helloStr = [72,69,76,76,79,161,161,161,161];    // HELLO in ascii
var worldStr = [87,79,82,76,68,33,33,33,33];        // WORLD in ascii
var i2cDevice = i2c.open({ bus: 0, speed: 100 });

var setup = GLCD_FS_ROWS_2 | GLCD_FS_DOT_SIZE_LITTLE | GLCD_FS_8BIT_MODE | GLCD_CMD_FUNCTION_SET;
msgData.writeUInt8(0,0);
msgData.writeUInt8(setup,1);
i2cDevice.write(lcdDispAddr,msgData);

setup = GLCD_DS_DISPLAY_ON | GLCD_DS_CURSOR_ON | GLCD_DS_BLINK_ON | GLCD_CMD_DISPLAY_SWITCH;
msgData.writeUInt8(0,0);
msgData.writeUInt8(setup,1);
i2cDevice.write(lcdDispAddr,msgData);

while (true) {
    // Reset the cursor position
    var col = 0;
    col |= 0x80;

    msgData.writeUInt8(GLCD_CMD_SET_DDRAM_ADDR,0);
    msgData.writeUInt8(col,1);
    i2cDevice.write({address: GROVE_LCD_DISPLAY_ADDR, length: 2},msgData);

    if (hello) {
        msgData.writeUInt8(GLCD_CMD_SET_CGRAM_ADDR,0);
        msgData.writeUInt8(helloStr[0],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(helloStr[1],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(helloStr[2],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(helloStr[3],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(helloStr[4],1);
        i2cDevice.write(lcdDispAddr,msgData);
        hello = false;
   }
   else {
        msgData.writeUInt8(GLCD_CMD_SET_CGRAM_ADDR,0);
        msgData.writeUInt8(worldStr[0],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(worldStr[1],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(worldStr[2],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(worldStr[3],1);
        i2cDevice.write(lcdDispAddr,msgData);
        msgData.writeUInt8(worldStr[4],1);
        i2cDevice.write(lcdDispAddr,msgData);
        hello = true;
   }
}
