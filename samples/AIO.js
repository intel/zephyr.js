// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the two onboard LEDs for output, and
// expects a button or similar input connected to digital pin 4.

print("Webbluetooth Demo with BLE...");

// import aio module
var aio = require("aio");

// pins
var pinA = aio.open({ device: 0, pin: 10 });
var pinB = aio.open({ device: 0, pin: 11 });

setInterval(function () {
    var rawValue = pinA.read();
    if (rawValue == 0) {
        print("PinA: invalid temperature value");
    } else {
        var voltage = (rawValue / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;
        print("PinA: temperature in Celsius is: " + celsius);
    }
}, 1000);

setInterval(function () {
    pinB.read_async(function(rawValue) {
        print("PinB - raw value is: " + rawValue);
    });
}, 1000);
