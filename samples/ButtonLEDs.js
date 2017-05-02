// Copyright (c) 2016-2017, Intel Corporation.

// Test code for Arduino 101 that uses the two onboard LEDs for output, and
// expects a button or similar input connected to IO4. Blinks the LEDs on and
// off together, but when the button is pressed turns one on and one off until
// the next timer expires.

console.log("GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");

// LED1 and LED2 are onboard LEDs on Arduino 101
var pinA = gpio.open({pin: "LED1", activeLow: true});
var pinB = gpio.open({pin: "LED2", activeLow: false});
var pinIn = gpio.open({pin: "IO4", mode: 'in', edge: 'rising'});

// tick is the delay between blinks
var tick = 1000, toggle = 0;

setInterval(function () {
    toggle = 1 - toggle;
    pinA.write(toggle);
    pinB.write(toggle);
}, tick);

pinIn.onchange = function(event) {
    pinA.write(1);
    pinB.write(0);
};
