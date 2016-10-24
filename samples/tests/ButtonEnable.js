// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses two buttons on IO2 and IO4 to control
// the two onboard LEDs.
//
// This version tests freeing a callback by changing the callback of btn2
// dynamically in response to btn1 state. Btn2 will only work to control its
// LED while btn1 is down.
// 
// Note: If you use a Grove touch sensor as one of the buttons, wire it to IO4:
// it doesn't work the other way and I haven't figured out why yet. It may
// relate to IO5 being connected to both the X86 and ARC processors.

console.log("Button enable test...");

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

var led1 = gpio.open({ pin: pins.LED0, activeLow: false });
var led2 = gpio.open({ pin: pins.LED1, activeLow: true });
var btn1 = gpio.open({ pin: pins.IO2, direction: 'in', edge: 'any' });
var btn2 = gpio.open({ pin: pins.IO4, direction: 'in', edge: 'any' });

// turn off LED #2 initially
led2.write(false);

btn1.on('change', function () {
    var value = btn1.read();
    led1.write(value);

    if (value) {
        // set up btn2 to toggle led2 while btn1 is down
        btn2.on('change', function () {
            var value = btn2.read();
            led2.write(value);
        });
    }
    else {
        // disable btn2 again
        btn2.on('change', null);
    }
});
