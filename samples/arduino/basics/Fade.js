// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino - Basics - Fade example
//   - Fades LEDs in and out using PWM
//   - IO3 uses normal polarity, IO5 uses reverse polarity, so they alternate

// Hardware Requirements:
//   - One or two LEDs with resistors
// Wiring:
//   For each external LED:
//     - Wire its long lead to the PWM pin you choose below (IO3/IO5 default)
//     - Wire its short lead to one end of a resistor
//     - Wire the other end of the resistor to ground
// Note: For a completely safe resistor size, find the LED's actual forward
//   voltage (or lowest reported), subtract from 3.3V, and divide by the
//   desired current. For example:
//     (3.3V - 1.8V) / 20 mA = 1.5 V / 0.02 A = 75 Ohms.
//   Larger resistors will make the LED dimmer. Smaller ones could reduce its
//     life.

console.log("Starting Fade example...");

var brightness = 0;
var fadeAmount = 5;
var delay = 30;  // time between adjustments (in ms)

// import pwm module
var pwm = require("pwm");

var led1 = pwm.open('IO3');
var led2 = pwm.open({pin: 'IO5', reversePolarity: true});

// update the brightness every 30ms
setInterval(function () {
    led1.setCycles(256, brightness);
    led2.setCycles(256, brightness);

    // adjust the brightness for next time
    brightness += fadeAmount;

    // reverse fade direction
    if (brightness == 0 || brightness == 255)
        fadeAmount = -fadeAmount;
}, delay);
