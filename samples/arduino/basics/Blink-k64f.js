// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Blink example
//   - Toggles an onboard LED on and off every second

// Hardware Requirements:
//   - None, to use the onboard LED
//   - Otherwise, an LED and a resistor (see basics/Blink.js for wiring info)

console.log("Starting Blink-k64f example...");

// import gpio module
var gpio = require("gpio");
var pins = require("k64f_pins");

// LEDG is the green LED within the RGB LED on the FRDM-K64F
// 'out' direction is default, could be left out
var pin = gpio.open({
    pin: pins.D5,
    direction: 'out'
});

// remember the current state of the LED
var toggle = false;

// schedule a function to run every 1s (1000ms)
setInterval(function () {
    toggle = !toggle;
    pin.write(toggle);
}, 1000);
