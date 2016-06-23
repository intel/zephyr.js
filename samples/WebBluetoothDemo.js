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
    var data = new Buffer(8);
    data.writeUInt8(this._lastValue);

    // BUG: this.RESULT_SUCCESS is undefined here
    callback(TemperatureCharacteristic.RESULT_SUCCESS, data);
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
    print("Temperature change " + value);

    var data = new Buffer(8);
    data.writeUInt8(newValue);

    if (this._onChange) {
        this._onChange(data);
    }
};

var ColorCharacteristic = new ble.Characteristic({
    uuid: 'fc0b',
    properties: ['read', 'write'],
    value: null
});

ColorCharacteristic._value = new Buffer(3);

ColorCharacteristic.onReadRequest = function(offset, callback) {
    print("Color onReadRequest called");
    // BUG: this.RESULT_SUCCESS and this._value is undefined here
    var data = new Buffer(3);
    callback(ColorCharacteristic.RESULT_SUCCESS, data);
};

ColorCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    print("Color onWriteRequest called");
    var value = data;
    if (!value) {
        // BUG: this.RESULT_SUCCESS is undefined here
        callback(ColorCharacteristic.RESULT_SUCCESS);
        return;
    }

    this._value = value;

    print("Changing led to " + value.toString('hex'));
    callback(ColorCharacteristic.RESULT_SUCCESS);
};

ble.on('stateChange', function(state) {
    print(state);

    if (state === 'poweredOn') {
        print("POWERED ON")
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
