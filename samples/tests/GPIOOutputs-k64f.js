// Copyright (c) 2017, Intel Corporation.

// Test code for FRDM-K64F that toggles each of pins D1-D15 as outputs
// indefinitely. Configures D0 as an input and prints a message whenever it
// changes, so you connect a jumper wire from D0 to each of the other pins (one
// at a time) and see which ones generate a console message.

console.log("Test external GPIOs as outputs...");

console.log("\nWire D0 to each of D1-D15 in turn!");
console.log("(D14-D15 are at the end of the 10-pin header)\n");

// Expected results: All pins but D8 currently show up with output events

// import gpio module
var gpio = require("gpio");

// blink D0 forever
var input = gpio.open({pin: 0, mode: 'in', edge: 'rising'});
var count = 1;
input.onchange = function(event) {
    console.log("Output event", count);
    count++;
}

// test all pins but D0
var testpins = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
var pincount = testpins.length;
var gpios = [];
for (var i = 0; i < pincount; i++) {
    gpios[i] = gpio.open({pin: testpins[i]});
}

var tick = 250, toggle = 0;
setInterval(function () {
    for (var i = 0; i < pincount; i++) {
        gpios[i].write(toggle);
    }
    toggle = 1 - toggle;
}, tick);
