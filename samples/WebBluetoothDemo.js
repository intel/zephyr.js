// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes

// import aio and ble module
var aio = require("aio");
var ble = require("ble");

DEVICE_NAME = 'Arduino101';

var TemperatureCharacteristic = new ble.Characteristic({
    uuid: 'fc0a',
    properties: ['read', 'notify'],
    value: null
});

TemperatureCharacteristic._lastValue = undefined;
TemperatureCharacteristic._onChange = null;

TemperatureCharacteristic.onReadRequest = function(offset, callback) {
    print("Temperature onReadRequest called");
};

var ColorCharacteristic = new ble.Characteristic({
    uuid: 'fc0b',
    properties: ['read', 'write'],
    value: null
});

ColorCharacteristic.onReadRequest = function(offset, callback) {
    print("Color onReadRequest called");
};

ColorCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    print("Color onWriteRequest called");
};

ble.on('stateChange', function(state, callback) {
    print(state);

    if (state === 'poweredOn') {
        ble.startAdvertising(DEVICE_NAME, ['fc00']);

        // FIXME: for now this will be hard-coded in C as part of above call
        //beacon.advertiseUrl("https://goo.gl/9FomQC", {name: DEVICE_NAME});
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
});

ble.on('disconnect', function(clientAddress) {
    print("Disconnected Connection: " + clientAddress);
});

print("Webbluetooth Demo with BLE...");

// enable ble
ble.enable();
