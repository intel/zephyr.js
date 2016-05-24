// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - Fade example
// * Fades an LED in and out using PWM

var brightness = 0;
var fadeAmount = 5;
var delay = 30;  // time between adjustments (in ms)

// import pwm module
var pwm = require("pwm");

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3,
//   so channel 3 here is IO9

// resolution on Arduino 101 is 31.25ns
var led = pwm.open({channel: 3, period: 255000, dutyCycle: 0});

// update the brightness every 30ms
setInterval(function () {
    print(brightness);
    led.setDutyCycle(brightness * 1000);

    brightness += fadeAmount;

    if (brightness == 0 || brightness == 255)
        fadeAmount = -fadeAmount;
}, delay);
