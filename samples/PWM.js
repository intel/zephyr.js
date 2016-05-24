// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that uses the PWM on pin IO3 to send turn on for
// 1s, off 1/2s, and so on.
print("PWM test...");

// import pwm module
var pwm = require("pwm");

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3
var channel0 = pwm.open({channel: 0, period: 1500000000,
                         dutyCycle: 1000000000});
