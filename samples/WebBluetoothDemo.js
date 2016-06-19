// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes

// import aio and ble module
var aio = require("aio");
var ble = require("ble");

print("Webbluetooth Demo with BLE...");

var serviceName = 'Arduino101';
var serviceUuid = 'fc00';
var tempUuid = 'fc0a';
var rgbUuid = 'fc0b';

// setup callbacks
ble.on('stateChange', function(state) {
    print('State change: ' + state);
    if (state === 'poweredOn') {
        //ble.startAdvertising();
        ble.startAdvertising(serviceName, [serviceUuid]);
    } else {
        ble.stopAdvertising();
    }
});

ble.on('accept', function(clientAddress) {
    print("Accepted connection from address: " + clientAddress);
});

ble.on('disconnect', function(clientAddress) {
    print("Disconnected from address: " + clientAddress);
});

ble.on('advertisingStart', function(error) {
    if (error) {
        print("Advertising start error: " + error);
        return;
    }
    print ("Advertising start");
    //ToDO: Define your new service
    ble.setServices([
        new ble.PrimaryService({
            uuid: serviceUuid,
            characteristics: [
                // TODO:
                // define our own characterstics
                new ble.Characteristic({
                    uuid: tempUuid,
                    properties: ['read', 'notify'],
                    secure: [],                      // not supported
                    value: null,                     // not supported
                    descriptors: [],                 // not supported
                    onReadRequest: function(offset, callback) {
                        print("OnReadRequest");
                        // TODO: implement
                        // var data = new Buffer(8);
                        // data.writeUInt8(this._lastValue);
                        // callback(this.RESULT_SUCCESS, data);
                    },
                    onWriteRequest: null,
                    onSubscribe: function(maxValueSize, updateValueCallback) {
                        print("Subscribed to temperature change.");
                        // TODO: implement
                    },
                    onUnsubscribe: function() {
                        print("Unsubscribed to temperature change.");
                        // TODO: implement
                    },
                    onNotify: null                   // not supported
                }),
                new ble.Characteristic({
                    uuid: rgbUuid,
                    properties: ['read', 'write'],
                    secure: [],                      // not supported
                    value: null,                     // not supported
                    descriptors: [],                 // not supported
                    onReadRequest: function(offset, callback) {
                        print("OnReadRequest");
                        // TODO: implement
                    },
                    onWriteRequest: function(data, offset, withoutResponse, callback) {
                        print("OnWriteRequest");
                        // TODO: implement
                    },
                    onSubscribe: null,               // not supported
                    onUnsubscribe: null,             // not supported
                    onNotify: null                   // not supported
                })
            ]
        })
    ]);
});

// enable ble
ble.enable();
