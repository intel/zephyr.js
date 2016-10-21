// Copyright (c) 2016, Intel Corporation.

// Sample code for showing how to to read raw input value from the analog
// pins on the Arduino 101, specifically A0 and A1, which is mapped
// to pin 10 and pin 11 on Zephyr, where one is doing a synchronous
// read and the other does it asynchronously

// import aio module
var aio = require("aio");
var pins = require("arduino101_pins");

// pins
var pinA = aio.open({ device: 0, pin: pins.A0 });
var pinB = aio.open({ device: 0, pin: pins.A1 });

setInterval(function () {
    var rawValue = pinA.read();
    if (rawValue == 0) {
        console.log("PinA: invalid temperature value");
    } else {
        var voltage = (rawValue / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;
        celsius = celsius | 0;
        console.log("PinA: temperature in Celsius is: " + celsius);
    }
}, 1000);

setInterval(function () {
    pinB.readAsync(function(rawValue) {
        console.log("PinB - raw value is: " + rawValue);
    });
}, 1000);
