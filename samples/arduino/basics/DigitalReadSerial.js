// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - DigitalReadSerial example
// * Reads a button state every second and prints it to the serial console

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

gpio.open({pin: pins.IO4, direction: 'in'}).then(function(pin) {
	// schedule a function to run every 1s (1000)
	setInterval(function () {
		print(pin.read());
	}, 1000);
}).docatch(function(error) {
	print("Error opening GPIO pin");
});
