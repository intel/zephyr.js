// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes

// Hardware Requirements:
//   - A temperature sensor (TMP36)
//   - A color LED bulb
//   - 200-ohm resisters
// Wiring:
//   For the temperature sensor:
//     - Wire the device's power to Arduino 3.3V and ground to GND
//     - Wire the signal pin to Arduino A0
//   For the color LED bulb:
//     - The LED has 4 legs, wire the longest one to the ground
//     - Wire the other 3 legs to 220-ohm resisters that connect to the
//         three PWM pins (IO3, IO5, IO6)

var aio = require("aio");
var ble = require("ble");
var pwm = require("pwm");
var pins = require("arduino101_pins");

var DEVICE_NAME = 'Arduino101';

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

ColorCharacteristic.ledR = pwm.open({
    channel: pins.IO3, period: 0.256, pulseWidth: 255 / 1000
});
ColorCharacteristic.ledG = pwm.open({
    channel: pins.IO5, period: 0.256, pulseWidth: 0
});
ColorCharacteristic.ledB = pwm.open({
    channel: pins.IO6, period: 0.256, pulseWidth: 0
});

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

    this.ledR.setPulseWidth(this._value.readUInt8(0) / 1000);
    this.ledG.setPulseWidth(this._value.readUInt8(1) / 1000);
    this.ledB.setPulseWidth(this._value.readUInt8(2) / 1000);
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

console.log("WebBluetooth Demo with LED Bulb...");
