// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the PWM on pin IO3 to set an LED's
// brightness, and the PWM on pin IO9 to set an LED's blink pattern to on for
// 1s, then off 1/2s, indefinitely.
print("PWM test for brightness and blink pattern...");

// import pwm module
var pwm = require("pwm");

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3
var led0 = pwm.open({channel: 0});  // pin IO3

// set timings in microseconds with *US functions
led0.setPeriodUS(1500000);
led0.setPulseWidthUS(1000000);

// set brightness to 33% using hw cycle-based values
var led1 = pwm.open({channel: 1, period: 3, pulseWidth: 1});  // pin IO5

// reproduce the Zephyr PWM sample in JS, changing blink timings every 4s
var led2 = pwm.open({channel: 2});  // pin IO6

var minPeriod = 1000;  // 1ms
var maxPeriod = 1000000;  // 1s
var period = maxPeriod;
var dir = 0;

// set initial state
led2.setPeriodUS(period);
led2.setPulseWidthUS(period / 2);

setInterval(function () {
    if (dir) {
        period *= 2;

        if (period > maxPeriod) {
            dir = 0;
            period = maxPeriod;
        }
    }
    else {
        period /= 2;

        if (period < minPeriod) {
            dir = 1;
            period = minPeriod;
        }
    }

    led2.setPeriodUS(period);
    led2.setPulseWidthUS(period / 2);
}, 4000);
