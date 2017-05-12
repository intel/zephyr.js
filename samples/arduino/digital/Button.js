// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino - Digital - Button example
//   - Turns on an LED whenever a button is being pressed

// Hardware Requirements
//   - A button, such as the Grove Kit's Button or Touch module
//   - Optionally, an external LED (see basics/Blink.js for wiring info)
// Wiring:
//   - Wire the device's power to Arduino 3.3V and ground to GND
//   - Wire the signal pin to IO4

console.log('Starting Button example...');

// import gpio module
var gpio = require('gpio');

var led = gpio.open({pin: 'LED0', mode: 'out', activeLow: true});
var button = gpio.open({pin: 'IO4', mode: 'in', edge: 'any'});

led.write(0);

button.onchange = function(event) {
    led.write(event.value);
}
