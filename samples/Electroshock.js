// Copyright (c) 2017, Intel Corporation.

// Sample code for Arduino 101 w/ Grove Button on IO2 and Grove Buzzer on IO4.
// The buzzer squeals while the button is held down. Some tape over the buzzer
// could probably make it a lot less annoying.

console.log("Electroshock sample...");

var pins = require("arduino101_pins");
var gpio = require("gpio");

var debounce = false;

var button = gpio.open({ pin: pins.IO2, direction: 'in', edge: 'any' });

// IO4, IO7, IO8 can work here
var buzzer = gpio.open({ pin: pins.IO4, direction: 'out' });

var lastState = false;

button.onchange = function (event) {
    if (debounce)
        return;

    debounce = true;

    updateBuzzer();

    setTimeout(resetDebounce, 20);
}

function resetDebounce() {
    debounce = false;
    updateBuzzer();
}

function updateBuzzer() {
    var value = button.read();
    if (value != lastState) {
        console.log("WRITE", value);
        buzzer.write(value);
        lastState = value;
    }
}
