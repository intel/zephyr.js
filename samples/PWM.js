// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the PWM on pin IO3 to set an LED's
// brightness, and the PWM on pin IO9 to set an LED's blink pattern to on for
// 1s, then off 1/2s, indefinitely.
print("PWM test for brightness and blink pattern...");

// import pwm module
var pwm = require("pwm");

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3
var led0 = pwm.open({channel: 3});  // pin IO9

// set timings in microseconds with *US functions
led0.setPeriodUS(1500000);
led0.setPulseWidthUS(1000000);

// set brightness to 33% using hw cycle-based values
var led1 = pwm.open({channel: 0, period: 3, pulseWidth: 1});  // pin IO3
