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
var pins = require("k64f_pins");

// blink D0 forever
var input = gpio.open({pin: pins.D0, direction: 'in', edge: 'rising'});
var count = 1;
input.onchange = function(event) {
    console.log("Output event", count);
    count++;
}

// test all pins but D0
var testpins = [pins.D1, pins.D2, pins.D3, pins.D4, pins.D5, pins.D6, pins.D7,
                pins.D8, pins.D9, pins.D10, pins.D11, pins.D12, pins.D13,
                pins.D14, pins.D15];
var pincount = testpins.length;
var gpios = []
for (var i = 0; i < pincount; i++) {
    gpios[i] = gpio.open({pin: testpins[i]});
}

var tick = 250, toggle = false;
setInterval(function () {
    for (var i = 0; i < pincount; i++) {
        gpios[i].write(toggle);
    }
    toggle = !toggle;
}, tick);
