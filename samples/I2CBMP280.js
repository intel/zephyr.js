// Copyright (c) 2016, Intel Corporation.

// Sample code demonstrating how to use I2C to communicate with slave devices

// Hardware Requirements:
//   - A BMP280 Temp/pressure sensor
//   - pull-up resistors for SDA and SCL, use two 10k resistors

// Wiring:
//   For LCD and BMP280:
//     - Wire SDA on the Arduino 101 to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the Arduino 101 to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD and BMP280 to SDA on the Arduino 101
//     - Wire SCL on the LCD and BMP280 to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

var i2c = require("i2c");

var bmp280 = {
    BMP280_ADDRESS: 0x77,
    BMP280_NORMAL_MODE: 0x03,
    BMP280_REGISTER_DIG_T1: 0x88, // Unsigned Short
    BMP280_REGISTER_DIG_T2: 0x8A, // Signed Short
    BMP280_REGISTER_DIG_T3: 0x8C, // Signed Short
    BMP280_REGISTER_CHIPID: 0xD0,
    BMP280_REGISTER_CONTROL: 0xF4,
    BMP280_REGISTER_CONFIG: 0xF5,
    BMP280_REGISTER_TEMPDATA: 0xFA, // Signed 20bit. Always positive
    BMP280_STANDBY: 2 << 5,
    BMP280_FILTER: 0x00,
    BMP280_SPI_3W_DISABLE: 0x00,
    BMP280_PRESS_OVER: 1 << 2,
    BMP280_TEMP_OVER: 1 << 5
}

var glcd = {
    GROVE_LCD_DISPLAY_ADDR : 0x3E,
    GROVE_RGB_BACKLIGHT_ADDR : 0x62,
// Defines for the GLCD_CMD_CURSOR_SHIFT
    GLCD_CS_DISPLAY_SHIFT : 1 << 3,
    GLCD_CS_RIGHT_SHIFT : 1 << 2,
// LCD Display Commands
    GLCD_CMD_SCREEN_CLEAR : 1 << 0,
    GLCD_CMD_CURSOR_RETURN : 1 << 1,
    GLCD_CMD_INPUT_SET : 1 << 2,
    GLCD_CMD_DISPLAY_SWITCH : 1 << 3,
    GLCD_CMD_CURSOR_SHIFT : 1 << 4,
    GLCD_CMD_FUNCTION_SET : 1 << 5,
    GLCD_CMD_SET_CGRAM_ADDR : 1 << 6,
    GLCD_CMD_SET_DDRAM_ADDR : 1 << 7,
// Defines for the LCD_FUNCTION_SET
    GLCD_FS_8BIT_MODE : 1 << 4,
    GLCD_FS_ROWS_2 : 1 << 3,
    GLCD_FS_ROWS_1 : 0 << 3,
    GLCD_FS_DOT_SIZE_BIG : 1 << 2,
    GLCD_FS_DOT_SIZE_LITTLE : 0 << 2,
// Defines for the GLCD_CMD_DISPLAY_SWITCH options
    GLCD_DS_DISPLAY_ON : 1 << 2,
    GLCD_DS_DISPLAY_OFF : 0 << 2,
    GLCD_DS_CURSOR_ON : 1 << 1,
    GLCD_DS_CURSOR_OFF : 0 << 1,
    GLCD_DS_BLINK_ON : 1 << 0,
    GLCD_DS_BLINK_OFF : 0 << 0,
    REGISTER_POWER : 0x08,
    REGISTER_R : 0x04,
    REGISTER_G : 0x03,
    REGISTER_B : 0x02
}

/* Globals */
var i2cDevice = i2c.open({ bus: 0, speed: 100 });
var dig_T1;
var dig_T2;
var dig_T3;
var baseTemp = prevTemp = 25;
var firstRead = true;
var red = green = blue = 200;

function uint16toInt16(value) {
    if ((value & 0x8000) > 0) {
        value = value - 0x10000;
    }
    return value;
}

function resetCursor(col, row) {
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

    // Write new cursor position via I2C
    i2cDevice.write(glcd.GROVE_LCD_DISPLAY_ADDR,
                    new Buffer([glcd.GLCD_CMD_SET_DDRAM_ADDR, col]));
}

function setColorFromTemperature(newTemp) {
    var diff = newTemp - baseTemp;
    var change = (diff > 0) ? (diff * 40) : (diff * -40);
    change = (change > 255) ? 255 : (change | 0);

    if (baseTemp < newTemp) {
        blue = (200 - change < 0) ? 0 : (200 - change);
        green = blue;
        red = (200 + change > 255) ? 255 : (200 + change);
    } else {
        red = (200 - change < 0) ? 0 : (200 - change);
        green = red;
        blue = (200 + change > 255) ? 255 : (200 + change);
    }

    prevTemp = newTemp;

    // Valid range for color is 0 - 255
    // Write new color values
    i2cDevice.write(glcd.GROVE_RGB_BACKLIGHT_ADDR,
                    new Buffer([glcd.REGISTER_B, blue]));
    i2cDevice.write(glcd.GROVE_RGB_BACKLIGHT_ADDR,
                    new Buffer([glcd.REGISTER_R, red]));
    i2cDevice.write(glcd.GROVE_RGB_BACKLIGHT_ADDR,
                    new Buffer([glcd.REGISTER_G, green]));
}

function writeText(word) {
    var wordBuffer = new Buffer(word.length + 1);

    // The first byte in the buffer is for the register address,
    // which is where the word is going to be written to.
    wordBuffer.writeUInt8(glcd.GLCD_CMD_SET_CGRAM_ADDR, 0);

    // Append the text we want to print after that
    wordBuffer.write(word, 1);

    // Write the buffer to I2C
    i2cDevice.write(glcd.GROVE_LCD_DISPLAY_ADDR, wordBuffer);
}

function init() {
    var reader = i2cDevice.read(bmp280.BMP280_ADDRESS, 1,
                                bmp280.BMP280_REGISTER_CHIPID);

    // Check that we have a BMP280 on the I2C bus
    if (reader.toString('hex') !== "58")
        console.log("BMP280 not found! ChipId " + reader.toString('hex') !== "58");

    readCoefficients();

    /* Setup BMP280 sensor */
    i2cDevice.write(bmp280.BMP280_ADDRESS,
                    new Buffer([bmp280.BMP280_REGISTER_CONTROL,
                                bmp280.BMP280_PRESS_OVER |
                                bmp280.BMP280_TEMP_OVER |
                                bmp280.BMP280_NORMAL_MODE]));

    i2cDevice.write(bmp280.BMP280_ADDRESS,
                    new Buffer([bmp280.BMP280_REGISTER_CONFIG,
                                bmp280.BMP280_STANDBY |
                                bmp280.BMP280_FILTER |
                                bmp280.BMP280_SPI_3W_DISABLE]));

    /* Setup Grove LCD */
    // Clear the LCD
    i2cDevice.write(glcd.GROVE_LCD_DISPLAY_ADDR,
                    new Buffer([0, glcd.GLCD_CMD_SCREEN_CLEAR]));

    // Set our preferences for the Grove LCD
    i2cDevice.write(glcd.GROVE_LCD_DISPLAY_ADDR,
                    new Buffer([0, glcd.GLCD_CMD_FUNCTION_SET |
                                   glcd.GLCD_FS_ROWS_2 |
                                   glcd.GLCD_FS_DOT_SIZE_LITTLE |
                                   glcd.GLCD_FS_8BIT_MODE]));

    i2cDevice.write(glcd.GROVE_LCD_DISPLAY_ADDR,
                    new Buffer([0, glcd.GLCD_CMD_DISPLAY_SWITCH |
                                   glcd.GLCD_DS_DISPLAY_ON |
                                   glcd.GLCD_DS_CURSOR_OFF |
                                   glcd.GLCD_DS_BLINK_OFF]));

    resetCursor(0, 0);
    writeText("Temperature is ");
}

function readCoefficients() {
    // These need to be read as a burst read in order to get accurate numbers
    dig_TAll = i2cDevice.burstRead(bmp280.BMP280_ADDRESS, 24,
                                   bmp280.BMP280_REGISTER_DIG_T1);
    dig_T1 = dig_TAll.readUInt16LE();
    dig_T2 = dig_TAll.readUInt16LE(2);
    dig_T3 = dig_TAll.readUInt16LE(4);

    // Convert these to signed
    dig_T1 = uint16toInt16(dig_T1);
    dig_T2 = uint16toInt16(dig_T2);
    dig_T3 = uint16toInt16(dig_T3);
}

function readTemperature() {
    // Read the raw temperature value.
    var tempBuffer = i2cDevice.burstRead(bmp280.BMP280_ADDRESS, 4,
                                         bmp280.BMP280_REGISTER_TEMPDATA);
    // Get the number from the buffer object
    var tempDec = tempBuffer.readUInt32BE();
    //Select the first 20 bits of 32 bit integer and maintains the sign
    tempDec >>>= 12;

    // Convert raw temperature to Celsius using the algorithm found here
    // https://www.bosch-sensortec.com/bst/products/all_products/bmp280
    var var1 = (((tempDec >> 3) - (dig_T1 << 1)) * (dig_T2)) >> 11;
    var var2 = (((((tempDec >> 4) - (dig_T1)) * ((tempDec >> 4) - (dig_T1))) >> 12) *
                (dig_T3)) >> 14;

    var t_fine = var1 + var2;
    var T = ((t_fine * 5 + 128) >> 8) / 100 ;

    console.log("Temp = " + T);
    var whole = T | 0;
    var dec = (T * 10) | 0;
    dec = dec - whole * 10;
    var tempStr = whole  + '.' + dec + "C";
    // Draw icon for direction temp has changed
    resetCursor(0, 1);
    if (prevTemp < T) {
        writeText("+");
    } else {
        writeText("-");
    }

    // Write out the temp to the LCD
    resetCursor(9, 1);
    writeText(tempStr);
    setColorFromTemperature(T);
    prevTemp = T;
}

init();
setInterval(readTemperature, 10);
