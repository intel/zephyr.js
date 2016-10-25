// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Blink example
//   - Toggles onboard LEDs as if they were a traffic light

// Hardware Requirements:
//   - None, to use the onboard LED
//   - Otherwise, three LEDs and resistors
// Wiring:
//   For an external LED:
//     - Wire its long lead to the IO pin you choose below
//     - Wire its short lead to one end of a resistor
//     - Wire the other end of the resistor to Arduino GND
// Note: For a completely safe resistor size, find the LED's actual forward
//   voltage (or lowest reported), subtract from 3.3V, and divide by the
//   desired current. For example:
//     (3.3V - 1.8V) / 20 mA = 1.5 V / 0.02 A = 75 Ohms.
//   Larger resistors will make the LED dimmer. Smaller ones could reduce its
//     life.

console.log("Starting TrafficLight example...");

// WARNING: These traffic light timings are appropriate for hummingbirds only

// import gpio module
var gpio = require("gpio");
var pins = require("arduino101_pins");

var red    = gpio.open({pin: pins.LED2, direction: 'out', activeLow: true});
var yellow = gpio.open({pin: pins.LED1, direction: 'out', activeLow: true});
var green  = gpio.open({pin: pins.LED0, direction: 'out', activeLow: false});

var elapsed = 0;

// turn red light on first
red.write(true);
yellow.write(false);
green.write(false);

// schedule a function to run every 1s (1000ms)
setInterval(function () {
    elapsed++;
    if (elapsed == 5) {
        // switch to yellow after 5s
        red.write(false);
        yellow.write(true);
    } else if (elapsed == 7) {
        // switch to green after 2s
        yellow.write(false);
        green.write(true);
    } else if (elapsed == 10) {
        // switch to red after 3s
        green.write(false);
        red.write(true);
        elapsed = 0;
    }
}, 1000);
