// Copyright (c) 2016, Intel Corporation.
// JavaScript library for the BMP280 sensor

function BMP280() {

    // API object
    var bmp280API = {};

    var i2c = require("i2c");
    bmp280API.i2cDevice = i2c.open({ bus: 0, speed: 100 });

    bmp280API.bmp280Addrs = {
        ADDRESS: 0x77,
        NORMAL_MODE: 0x03,
        REGISTER_DIG_T1: 0x88, // Unsigned Short
        REGISTER_DIG_T2: 0x8A, // Signed Short
        REGISTER_DIG_T3: 0x8C, // Signed Short
        REGISTER_CHIPID: 0xD0,
        REGISTER_CONTROL: 0xF4,
        REGISTER_CONFIG: 0xF5,
        REGISTER_TEMPDATA: 0xFA, // Signed 20bit. Always positive
        STANDBY: 2 << 5,
        FILTER: 0x00,
        SPI_3W_DISABLE: 0x00,
        PRESS_OVER: 1 << 2,
        TEMP_OVER: 1 << 5
    }

    var dig_T1;
    var dig_T2;
    var dig_T3;

    function uint16toInt16(value) {
        if ((value & 0x8000) > 0) {
            value = value - 0x10000;
        }
        return value;
    }

    bmp280API.readCoefficients = function() {
        // These need to be read as a burst read in order to get accurate numbers,
        // so dig_TAll contains dig_T1 through dig_T3
        var dig_TAll = this.i2cDevice.burstRead(this.bmp280Addrs.ADDRESS, 24,
                                                this.bmp280Addrs.REGISTER_DIG_T1);
        dig_T1 = dig_TAll.readUInt16LE();
        dig_T2 = dig_TAll.readUInt16LE(2);
        dig_T3 = dig_TAll.readUInt16LE(4);

        // Convert these to signed
        dig_T1 = uint16toInt16(dig_T1);
        dig_T2 = uint16toInt16(dig_T2);
        dig_T3 = uint16toInt16(dig_T3);
    }

    bmp280API.readTemperature = function() {
        // Read the raw temperature value.
        var tempBuffer = this.i2cDevice.burstRead(this.bmp280Addrs.ADDRESS, 4,
                                                  this.bmp280Addrs.REGISTER_TEMPDATA);
        // Get the number from the buffer object
        var tempDec = tempBuffer.readUInt32BE();

        // Selects the first 20 bits of 32 bit integer and maintains the sign
        tempDec >>>= 12;

        // Convert raw temperature to Celsius using the algorithm found here
        // https://www.bosch-sensortec.com/bst/products/all_products/bmp280
        var var1 = (((tempDec >> 3) - (dig_T1 << 1)) * (dig_T2)) >> 11;
        var var2 = (((((tempDec >> 4) - (dig_T1)) * ((tempDec >> 4) - (dig_T1))) >> 12) *
                    (dig_T3)) >> 14;

        var t_fine = var1 + var2;
        var T = ((t_fine * 5 + 128) >> 8) / 100 ;

        return T;
    }

    return bmp280API;
};

module.exports.BMP280 = new BMP280();
