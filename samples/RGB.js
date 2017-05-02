// Copyright (c) 2016-2017, Intel Corporation.

// Sample to drive an RGB LED, namely this one:
//   http://www.mouser.com/ds/2/216/WP154A4SUREQBFZGW-67444.pdf
// The LED has four pins. The longest one is wired to ground, the other ones
//   should be connected to 220-ohm resistors that then connect to the three
//   R/G/B pins below (IO2, IO7, IO8).

// This demo cycles through eight color states on the LED, corresponding to
// all combinations of full off / full on for the three colors. It gives you
// something like off, red, blue, purple, green, orange, light blue, "white"
// (which looks suspiciously like purple/magenta).

// Also toggles an onboard LED off and on faster for good measure.

console.log("RGB LED sample...");

// import gpio module
var gpio = require("gpio");

// LED0 is an onboard LED on the Arduino101
var led = gpio.open({pin: "LED0", activeLow: true});

var red = gpio.open({pin: "IO2"});
var green = gpio.open({pin: "IO7"});
var blue = gpio.open({pin: "IO8"});

var count = 0;

// schedule a function to run every 0.5s
setInterval(function () {
    count += 1;
    led.write(count & 1);
    red.write(count & 2);
    green.write(count & 4);
    blue.write(count & 8);
}, 500);
