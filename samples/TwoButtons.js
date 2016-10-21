// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses two buttons on IO3 and IO4 to control
// the two onboard LEDs.
//
// Note: If you use a Grove touch sensor as one of the buttons, wire it to IO4:
// it doesn't work the other way and I haven't figured out why yet. It may
// relate to IO5 being connected to both the X86 and ARC processors.

console.log("GPIO test with two buttons controlling two LEDs...");

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

// pins 8 (LED0) and 12 (LED1) are onboard LEDs on Arduino 101
var led1 = gpio.open({ pin: pins.LED0, activeLow: false });
var led2 = gpio.open({ pin: pins.LED1, activeLow: true });
var btn1 = gpio.open({ pin: pins.IO3, direction: 'in', edge: 'any' });
var btn2 = gpio.open({ pin: pins.IO4, direction: 'in', edge: 'any' });

// turn off LED #2 initially
led2.write(false);

btn1.onchange = function (event) {
    led1.write(event.value);
};

btn2.onchange = function (event) {
    led2.write(event.value);
};
