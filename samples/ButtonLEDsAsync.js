// Copyright (c) 2016-2017, Intel Corporation.

// This is an asynchronous version of ButtonLEDs.js, calling gpio.openAsync

// Test code for Arduino 101 that uses the two onboard LEDs for output, and
// expects a button or similar input connected to IO4. Blinks the LEDs on and
// off together, but when the button is pressed turns one on and one off until
// the next timer expires.

console.log("Async GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

// LED1 and LED2 are onboard LEDs on Arduino 101
var pinA = null;
var pinB = null;

gpio.openAsync({pin: pins.LED1, activeLow: true}).then(function(pin) {
    pinA = pin;
});

gpio.openAsync({pin: pins.LED2, activeLow: false}).then(function(pin) {
    pinB = pin;
});

gpio.openAsync({pin: pins.IO4, direction: 'in', edge: 'rising'}).then(function(pin) {
    // tick is the delay between blinks
    var tick = 1000, toggle = false;

    setInterval(function () {
        toggle = !toggle;
        pin.read();
        pinA.write(toggle);
        pinB.write(toggle);
    }, tick);

    pin.onchange = function(event) {
        pinA.write(true);
        pinB.write(false);
    };
}).catch(function(error) {
    console.log("Error opening GPIO pin");
});
