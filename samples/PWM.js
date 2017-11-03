// Copyright (c) 2016-2017, Intel Corporation.

// Test code for Arduino 101 that uses all four PWMs. The ones on IO3 and IO5
// set LED brightnesses to 66% and 33%. The one on IO6 sets an LED's blink
// pattern to on for 1s, then off 1/2s, indefinitely. Another PWM on IO9
// oscillates an LED through a series of blink periods.
console.log('PWM test for brightness and blink patterns...');

// import pwm module
var pwm = require('pwm');

// pins 3, 5, 6, and 9 are PWMs on Arduino 101, and map to channels 0-3

// set brightness to 66% using hw cycle-based values
var led0 = pwm.open(0);  // same as '0', 'PWM0', 'IO3', or 'PWM_0.0'
led0.setCycles(100, 66);

// set brightness to 33% using hw cycle-based values
var led1 = pwm.open('1');  // same as 1, 'PWM1', 'IO5', or 'PWM_0.1'
led1.setCycles(100, 33);

var led2 = pwm.open('PWM2');  // same as 2, '2', 'IO6', or 'PWM_0.2'

// set timings in milliseconds
led2.setMilliseconds(1500, 1000);

// reproduce the Zephyr PWM sample in JS, changing blink timings every 4s
var led3 = pwm.open('IO9');  // same as 3, '3', 'PWM3', or 'PWM_0.3'

// The fifth way to refer to a PWM pin 'PWM_0.3' has the Zephyr device name
// before the dot, the pwm channel number after the dot. On Arduino 101, there
// is only one PWM controller 'PWM_0' and the pwm channels are 0-3. This can
// be used even on Zephyr boards that aren't yet specifically supported by ZJS.

var minPeriod = 1;  // 1ms
var maxPeriod = 1000;  // 1s
var period = maxPeriod;
var dir = 0;

// set initial state
led3.setMilliseconds(period, period / 2);

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

    led3.setMilliseconds(period, period / 2);
}, 4000);
