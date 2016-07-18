// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Digital - Button example
// * Turns on an LED whenever a button is being pressed

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

var led = gpio.open({pin: pins.LED0, direction: 'out'});
var button = gpio.open({pin: pins.IO4, direction: 'in', edge: 'any'});

button.onChange = function(event) {
    print("Button changed");
}
