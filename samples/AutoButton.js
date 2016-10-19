// Copyright (c) 2016, Intel Corporation.

// Sample simulates a button click by toggling a GPIO output up and down;
// Displays the iteration count every 10ms and should run indefinitely.

// This is a useful template for testing for a memory leak or stability
// problem by counting the number of times it can successfully run before
// locking up. If "exit" is printed on the serial console, generally this
// means JerryScript has run out of memory.

// Hardware Requirements:
//   - Arduino 101
// Wiring:
//   - Wire IO11 to IO12

console.log("Starting AutoButton example...");

// time between iterations (in milliseconds)
var delay = 10;

// import modules
var gpio = require("gpio");
var pins = require("arduino101_pins");

// listen for button presses on this input
var input = gpio.open({
    pin: pins.IO11,
    direction: 'in',
    edge: 'rising'
});

// simulate button presses on this output
var output = gpio.open({
    pin: pins.IO12,
    direction: 'out'
});

var count = 0;

input.onchange = function(event) {
    count += 1;
    console.log("Iteration #" + count);
    // add code here to test for problems
}

setInterval(function () {
    // simulate quick button press
    output.write(false);
    output.write(true);
}, delay);
