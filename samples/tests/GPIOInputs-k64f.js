// Copyright (c) 2017, Intel Corporation.

// Test code for FRDM-K64F that listens on pins D1-D15 as inputs and prints a
// message whenever one changes. Blinks D0 indefinitely, so you connect a
// jumper wire from D0 to each of the other pins (one at a time) and see which
// ones generate a console message.

console.log("Test external GPIOs as inputs...");

console.log("\nWire D0 to each of D1-D15 in turn!");
console.log("(D14-D15 are at the end of the 10-pin header)\n");

// Expected results: All pins but D8 currently show up with input events

// import gpio module
var gpio = require("gpio");

// blink D0 forever
var tick = 250, toggle = 0;
var output = gpio.open({pin: 0});
setInterval(function () {
    output.write(toggle);
    toggle = 1 - toggle;
}, tick);

// test all pins but D0
var testpins = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
var pincount = testpins.length;

var count = 1;
var gpios = [];
for (var i = 0; i < pincount; i++) {
    gpios[i] = gpio.open({pin: testpins[i], mode: 'in', edge: 'rising'});
    gpios[i].onchange = function(event) {
        console.log("Input event", count);
        count++;
    }
}
