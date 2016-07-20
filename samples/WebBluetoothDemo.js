// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes

// import aio and ble module
var aio = require("aio");
var ble = require("ble");
var pwm = require("pwm");
var pins = require("arduino101_pins");

DEVICE_NAME = 'Arduino101';

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

var pinA0 = aio.open({ device: 0, pin: pins.A0 });

TemperatureCharacteristic.onReadRequest = function(offset, callback) {
    var data = new Buffer(1);
    data.writeUInt8(this._lastValue);
    callback(this.RESULT_SUCCESS, data);
};

TemperatureCharacteristic.onSubscribe = function(maxValueSize,
                                                 updateValueCallback) {
    print("Subscribed to temperature change.");
    this._onChange = updateValueCallback;
    this._lastValue = undefined;
};

TemperatureCharacteristic.onUnsubscribe = function() {
    print("Unsubscribed to temperature change.");
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

// default color
ColorCharacteristic._value = new Buffer(3);
ColorCharacteristic._value.writeUInt8(255, 0);
ColorCharacteristic._value.writeUInt8(0, 1);
ColorCharacteristic._value.writeUInt8(0, 2);
ColorCharacteristic.ledR = pwm.open({channel: pins.IO3, period: 0.256,
                                     pulseWidth: 0.128});
ColorCharacteristic.ledG = pwm.open({channel: pins.IO5, period: 0.256,
                                     pulseWidth: 0});
ColorCharacteristic.ledB = pwm.open({channel: pins.IO6, period: 0.256,
                                     pulseWidth: 0});

ColorCharacteristic.onReadRequest = function(offset, callback) {
    print("led value: " + this._value.toString('hex'));
    callback(this.RESULT_SUCCESS, this._value);
};

ColorCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    var value = data;
    if (!value) {
        print("Error - color onWriteRequest: buffer not available");
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
    callback(this.RESULT_SUCCESS);
};

ble.on('stateChange', function(state) {
    print(state);

    if (state === 'poweredOn') {
        print("POWERED ON")
        ble.startAdvertising(DEVICE_NAME, ['fc00'], "https://goo.gl/9FomQC");
    } else {
        if (state === 'unsupported'){
            print("BLE and Bleno configurations not enabled on board");
        }
        ble.stopAdvertising();
    }
});

ble.on('advertisingStart', function(error) {
    print('advertisingStart: ' + (error ? error : 'success'));

    if (error) {
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
});

ble.on('accept', function(clientAddress) {
    print("Accepted Connection: " + clientAddress);

    pinA0.on("change", function(data) {
        var voltage = (data / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;

        print("Temperature change " + celsius);
        TemperatureCharacteristic.valueChange(celsius);
    });
});

ble.on('disconnect', function(clientAddress) {
    print("Disconnected Connection: " + clientAddress);

    pinA0.on("change", null);
});

print("WebBluetooth Demo with BLE...");

// Set magic value for now to get the main loop to sleep between callbacks
zjs_sleep = 100;
