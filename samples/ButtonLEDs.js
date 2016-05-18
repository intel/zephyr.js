// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the two onboard LEDs for output, and
// expects a button or similar input connected to digital pin 4.

print("GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");

// pins 8 and 12 are onboard LEDs on Arduino 101, pin 19 is IO4
var pinA = gpio.open({ pin: 8, activeLow: false });
var pinB = gpio.open({ pin: 12, activeLow: true });
var pinIn = gpio.open({ pin: 19, direction: 'in', edge: 'rising' });

// tick is the delay between blinks
var tick = 1000, toggle = false;

setInterval(function () {
    toggle = !toggle;
    pinIn.read();
    pinA.write(toggle);
    pinB.write(toggle);
}, tick);

gpio.set_callback(19, function () {
    pinA.write(true);
    pinB.write(false);
});
