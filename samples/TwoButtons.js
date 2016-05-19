// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses two buttons on IO4 and IO5 to control
// the two onboard LEDs.
//
// Note: If you use a Grove touch sensor as one of the buttons, wire it to IO4:
// it doesn't work the other way and I haven't figured out why yet. It may
// relate to IO5 being connected to both the X86 and ARC processors.

print("GPIO test with two buttons controlling two LEDs...");

// import gpio module
var gpio = require("gpio");

// pins 8 and 12 are onboard LEDs on Arduino 101, pin 19 is IO4
var led1 = gpio.open({ pin: 8, activeLow: false });
var led2 = gpio.open({ pin: 12, activeLow: true });
var btn1 = gpio.open({ pin: 15, direction: 'in', edge: 'any' });
var btn2 = gpio.open({ pin: 19, direction: 'in', edge: 'any' });

// turn off LED #2 initially
led2.write(false);

gpio.set_callback(15, function () {
    var val = btn1.read();
    led1.write(val);
});

gpio.set_callback(19, function () {
    var val = btn2.read();
    led2.write(val);
});
