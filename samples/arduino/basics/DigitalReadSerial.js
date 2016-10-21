// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - DigitalReadSerial example
//   - Reads a button state every second and prints it to the serial console

// Hardware Requirements
//   - A button, such as the Grove Kit's Button or Touch module
// Wiring:
//   - Wire the device's power to Arduino 3.3V and ground to GND
//   - Wire the signal pin to IO4

console.log("Starting DigitalReadSerial example...");

var gpio = require("gpio");
var pins = require("arduino101_pins");

var pin = gpio.open({
    pin: pins.IO4,
    direction: 'in'
});

// schedule a function to run every 1s (1000)
setInterval(function () {
    // read digital input pin IO4
    var value = pin.read();

    // print it to the serial console
    console.log(value);
}, 1000);
