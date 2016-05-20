// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the two onboard LEDs for output, and
// expects a button or similar input connected to digital pin 4.

print("Webbluetooth Demo with BLE...");

// import aio module
var aio = require("aio");

// pins
var pinIn = aio.open({ device: 0, pin: 10 });

setInterval(function () {
    var temp = pinIn.read();
    print("Temperature in Celsius is: " + temp);
}, 1000);
