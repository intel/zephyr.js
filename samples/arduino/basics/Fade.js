// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Fade example
// * Fades an LED in and out using PWM

var brightness = 0;
var fadeAmount = 5;
var delay = 30;  // time between adjustments (in ms)

// import pwm module
var pwm = require("pwm");
var pins = require("arduino101_pins");

var led = pwm.open({channel: pins.IO3, polarity: "reverse"});
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
