// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino - Basics - Blink example
//   - Toggles an onboard LED on and off every second

// Hardware Requirements:
//   - None, to use the onboard LED

console.log('Starting Blink example...');

// import gpio module
var gpio = require('gpio');

// GPIOC is the port name and pin 13 is the green LED on
// OLIMEX-STM32-E407 board
var pin = gpio.open({pin: 'GPIOC.13', mode: 'out', activeLow: true});

// remember the current state of the LED
var toggle = 0;

// schedule a function to run every 1s (1000ms)
setInterval(function () {
    toggle = 1 - toggle;
    pin.write(toggle);
}, 1000);
