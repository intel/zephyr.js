// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Blink example
// * Toggles an onboard LED on and off every second

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

// pin 8 is one of the onboard LEDs on the Arduino 101
// 'out' direction is default, could be left out
gpio.open({pin: pins.LED0, direction: 'out'}).then(function(pin) {
	// remember the current state of the LED
	var toggle = false;

	// schedule a function to run every 1s (1000ms)
	setInterval(function () {
		toggle = !toggle;
		pin.write(toggle);
	}, 1000);
}).docatch(function(error) {
	print("Error opening GPIO pin");
});
