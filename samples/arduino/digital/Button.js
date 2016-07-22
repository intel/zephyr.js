// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Digital - Button example
// * Turns on an LED whenever a button is being pressed

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

var led = null;

gpio.open({pin: pins.LED0, direction: 'out'}).then(function(pin) {
    led = pin;
}).docatch(function(error) {
    print("Error opening LED GPIO pin");
});

gpio.open({pin: pins.IO4, direction: 'in', edge: 'any'}).then(function(pin) {
    pin.onchange = function(event) {
        led.write(event.value);
    };
}).docatch(function(error) {
    print("Error opening GPIO pin");
});
