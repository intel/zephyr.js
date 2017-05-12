// Copyright (c) 2016-2017, Intel Corporation.

// Test code for FRDM-K64F that uses the onboard RGB LED for output, and
// onboard input button SW2. Blinks the LEDs on and off together, but when the
// button is pressed the red light turns off and blue light turns on, until the
// next timer expires.

console.log("GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");

// D0 - D15 will also work as these output pins, if you hook up an LED
// (except D8 currently not working on Rev E3 board at least)
var ledr = gpio.open({pin: "LEDR"});
var ledb = gpio.open({pin: "LEDB"});

// D0 - D15 will also work as this input pin, if you hook up a button
// (except D8 currently not working on Rev E3 board at least)
var pinIn = gpio.open({pin: "SW2", mode: 'in', edge: 'falling'});

// tick is the delay between blinks
var tick = 1000, toggle = 0;

setInterval(function () {
    toggle = 1 - toggle;
    ledr.write(toggle);
    ledb.write(toggle);
}, tick);

pinIn.onchange = function(event) {
    ledr.write(1);
    ledb.write(0);
};
