// Copyright (c) 2016-2017, Intel Corporation.

// Toggles onboard LEDs as if they were a traffic light

// Hardware Requirements:
//   - Arduino 101 (if using default LED0-2 pins)
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

console.log("Starting TrafficLight sample...");

// WARNING: These traffic light timings are appropriate for hummingbirds only

// import gpio module
var gpio = require("gpio");

var red    = gpio.open({pin: "LED0", activeLow: true});
var yellow = gpio.open({pin: "LED1", activeLow: true});
var green  = gpio.open({pin: "LED2", activeLow: false});

var elapsed = 0;

// turn red light on first
red.write(1);
yellow.write(0);
green.write(0);

// schedule a function to run every 1s (1000ms)
setInterval(function () {
    elapsed++;
    if (elapsed == 5) {
        // switch to green after 5s
        red.write(0);
        green.write(1);
    } else if (elapsed == 8) {
        // switch to yellow after 3s
        green.write(0);
        yellow.write(1);
    } else if (elapsed == 10) {
        // switch to red after 2s
        yellow.write(0);
        red.write(1);
        elapsed = 0;
    }
}, 1000);
