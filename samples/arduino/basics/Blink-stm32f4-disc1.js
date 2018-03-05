// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino - Basics - Blink example
//   - Toggles an onboard LEDs on and off every second

// Hardware Requirements:
//   - None, to use the onboard LEDs

console.log('Starting Blink example...');

// import gpio module
var gpio = require('gpio');

// There are four user onboard LEDs on the STM32F4DISCOVERY
var green = gpio.open({pin: 'GPIOD.12', mode: 'out', activeLow: true});
var orange = gpio.open({pin: 'GPIOD.13', mode: 'out', activeLow: true});
var red = gpio.open({pin: 'GPIOD.14', mode: 'out', activeLow: true});
var blue = gpio.open({pin: 'GPIOD.15', mode: 'out', activeLow: true});

// remember the current state of the LED
var toggle = 0;

// schedule a function to run every 1s (1000ms)
setInterval(function () {
    toggle = 1 - toggle;
    green.write(toggle);
    orange.write(toggle);
    red.write(toggle);
    blue.write(toggle);
}, 1000);
