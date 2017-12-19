// Copyright (c) 2016-2017, Intel Corporation.

// Sample code for showing how to to read raw input value from the analog
// pins on FRDM_K64f board or the Arduino 101, specifically A0 and A1,
// where one is doing a synchronous read and the other does it asynchronously.

// import aio module
var aio = require("aio");

// pins
var pinA = aio.open({ pin: 'A0' });
var pinB = aio.open({ pin: 'A1' });

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
