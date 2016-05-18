// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - DigitalReadSerial example
// * Reads a button state every second and prints it to the serial console

// import gpio module
var gpio = require("gpio");

// pin 19 corresponds to IO4 on the Arduino 101
var pin = gpio.open({pin: 19, direction: 'in'});

// schedule a function to run every 1s (1000)
setInterval(function () {
    print(pin.read());
}, 1000);
