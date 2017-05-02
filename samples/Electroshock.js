// Copyright (c) 2017, Intel Corporation.

// Sample code for Arduino 101 w/ Grove Button on IO2 and Grove Buzzer on IO4.
// The buzzer squeals while the button is held down. Some tape over the buzzer
// could probably make it a lot less annoying.

console.log("Electroshock sample...");

var gpio = require("gpio");

var button = gpio.open({pin: 2, mode: 'in', edge: 'any'});

// IO4, IO7, IO8 can work here
var buzzer = gpio.open({pin: 4});

var debounce = false;
var lastState = 0;

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
        buzzer.write(!!value);
        lastState = value;
    }
}
