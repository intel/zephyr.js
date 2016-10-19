// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes
// this version uses the Grove LCD instead of the PWM bulb

// Hardware Requirements:
//   - A temperature sensor (TMP36)
//   - A Grove LCD
//   - Two pull-up resistors for SDA and SCL, we use two 10k resistors,
//       you should choose resistors that will work with the LCD hardware
//       you have, the ones we listed here are the ones that have known
//       to work for us, so your mileage may vary if you have different LCD
// Wiring:
//   For the temperature sensor:
//     - Wire the device's power to Arduino 3.3V and ground to GND
//     - Wire the signal pin to Arduino A0
//   For the LCD:
//     - Wire SDA on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD to SDA on the Arduino 101
//     - Wire SCL on the LCD to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

var aio = require("aio");
var ble = require("ble");
var grove_lcd = require("grove_lcd");
var pins = require("arduino101_pins");

var DEVICE_NAME = 'Arduino101';

var glcd = grove_lcd.init();

var funcConfig = grove_lcd.GLCD_FS_ROWS_2
               | grove_lcd.GLCD_FS_DOT_SIZE_LITTLE
               | grove_lcd.GLCD_FS_8BIT_MODE;
glcd.setFunction(funcConfig);

var displayStateConfig = grove_lcd.GLCD_DS_DISPLAY_ON;
glcd.setDisplayState(displayStateConfig);

glcd.clear();
glcd.print("Web BLE Demo");

var TemperatureCharacteristic = new ble.Characteristic({
    uuid: 'fc0a',
    properties: ['read', 'notify'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'Temperature'
        })
    ]
});

TemperatureCharacteristic._lastValue = undefined;
TemperatureCharacteristic._onChange = null;

var tmp36 = aio.open({ device: 0, pin: pins.A0 });

TemperatureCharacteristic.onReadRequest = function(offset, callback) {
    if (!this._lastValue) {
        var rawValue = pinA0.read();
        if (rawValue == 0) {
            console.log("PinA: invalid temperature value");
            callback(this.RESULT_UNLIKELY_ERROR, null);
            return;
        }

        var voltage = (rawValue / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;
        this._lastValue = celsius;
    }

    console.log("Temperature: " + this._lastValue);
    var data = new Buffer(1);
    data.writeUInt8(this._lastValue);
    callback(this.RESULT_SUCCESS, data);
};

TemperatureCharacteristic.onSubscribe = function(maxValueSize,
                                                 updateValueCallback) {
    console.log("Subscribed to temperature change");
    this._onChange = updateValueCallback;
    this._lastValue = undefined;
};

TemperatureCharacteristic.onUnsubscribe = function() {
    console.log("Unsubscribed to temperature change");
    this._onChange = null;
};

TemperatureCharacteristic.valueChange = function(value) {
    this._lastValue = value;
    glcd.setCursorPos(0, 0);
    glcd.print("Temperature: " + value + "C");

    var data = new Buffer(1);
    data.writeUInt8(value);

    if (this._onChange) {
        this._onChange(data);
    }
};

var ColorCharacteristic = new ble.Characteristic({
    uuid: 'fc0b',
    properties: ['read', 'write'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'LED'
        })
    ]
});

// Default color: red.
ColorCharacteristic._value = new Buffer(3);
ColorCharacteristic._value.writeUInt8(255, 0);
ColorCharacteristic._value.writeUInt8(0, 1);
ColorCharacteristic._value.writeUInt8(0, 2);

glcd.setColor(255, 0, 0);

ColorCharacteristic.onReadRequest = function(offset, callback) {
    console.log("Color change: #" + this._value.toString('hex'));
    callback(this.RESULT_SUCCESS, this._value);
};

ColorCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    var value = data;
    if (!value) {
        console.log("Error - color onWriteRequest: buffer not available");
        callback(this.RESULT_UNLIKELY_ERROR);
        return;
    }

    this._value = value;
    if (this._value.length !== 3) {
        callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
        return;
    }

    var r = this._value.readUInt8(0);
    var g = this._value.readUInt8(1);
    var b = this._value.readUInt8(2);
    glcd.setColor(r, g, b);

    // TODO: probably only supposed to call this if withoutResponse is false?
    callback(this.RESULT_SUCCESS);
};

ble.on('stateChange', function(state) {
    if (state === 'poweredOn') {
        ble.startAdvertising(DEVICE_NAME, ['fc00'], "https://goo.gl/QEvyDZ");
    } else {
        if (state === 'unsupported') {
            console.log("BLE not enabled on board");
        }
        ble.stopAdvertising();
    }
});

ble.on('advertisingStart', function(error) {
    if (error) {
        console.log("Failed to advertise Physical Web URL");
        return;
    }

    ble.setServices([
        new ble.PrimaryService({
            uuid: 'fc00',
            characteristics: [
                TemperatureCharacteristic,
                ColorCharacteristic
            ]
        })
    ]);

    console.log("Advertising as Physical Web device");
});

ble.on('accept', function(clientAddress) {
    console.log("Client connected: " + clientAddress);
    glcd.clear();

    var lastTemp = 0;

    tmp36.on("change", function(data) {
        var voltage = (data / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;
        celsius = celsius | 0;

        if (celsius !== lastTemp) {
            lastTemp = celsius;
            console.log("Temperature change: " + celsius + "C");
            TemperatureCharacteristic.valueChange(celsius);
        }
    });
});

ble.on('disconnect', function(clientAddress) {
    console.log("Client disconnected: " + clientAddress);

    tmp36.on("change", null);
});

console.log("WebBluetooth Demo with Grove LCD...");
