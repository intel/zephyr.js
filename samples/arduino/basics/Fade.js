// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Fade example
//   - Fades an LED in and out using PWM

// Hardware Requirements:
//   - An LED and a resistor
// Wiring:
//   For an external LED:
//     - Wire its long lead to the PWM pin you choose below
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
var pins = require("arduino101_pins");

var led = pwm.open({
    channel: pins.IO3,
    polarity: "reverse"
});
led.setPeriodCycles(256);

// update the brightness every 30ms
setInterval(function () {
    led.setPulseWidthCycles(brightness);

    // adjust the brightness for next time
    brightness += fadeAmount;

    // reverse fade direction
    if (brightness == 0 || brightness == 255)
        fadeAmount = -fadeAmount;
}, delay);
