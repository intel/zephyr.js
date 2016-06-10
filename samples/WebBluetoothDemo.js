// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes

// import aio and ble module
var aio = require("aio");
var ble = require("ble");

print("Webbluetooth Demo with BLE...");

var serviceName = 'Arduino-101';
var serviceUuid = 'fffffffffffffffffffffffffffffff0';

// setup callbacks
ble.on('stateChange', function(state) {
    print ('State change: ' + state);
    if (state === 'poweredOn') {
        ble.startAdvertising();
        //ble.startAdvertising();
    } else {
        ble.stopAdvertising();
    }
});

ble.on('accept', function(clientAddress) {
    print ("Accepted connection from address: " + clientAddress);
});

ble.on('disconnect', function(clientAddress) {
    print ("Disconnected from address: " + clientAddress);
});

ble.on('advertisingStart', function(error) {
    if (error) {
        print ("Advertising start error: " + error);
        return;
    }
    print ("Advertising start success");
    //ToDO: Define your new service
});

// enable ble
ble.enable();
