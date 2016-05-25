// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Fade example
// * Fades an LED in and out using PWM

var brightness = 0;
var fadeAmount = 5;
var delay = 30;  // time between adjustments (in ms)

// import pwm module
var pwm = require("pwm");

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3,
//   so channel 3 here is IO9; default period of PWM Is 255
var led = pwm.open({channel: 3, polarity: "reverse"});

// update the brightness every 30ms
setInterval(function () {
    led.setPulseWidth(brightness);

    // adjust the brightness for next time
    brightness += fadeAmount;

    // reverse fade direction
    if (brightness == 0 || brightness == 255)
        fadeAmount = -fadeAmount;
}, delay);
