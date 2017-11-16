// Copyright (c) 2017, Intel Corporation.

// Test code for multiple boards that toggles each of the GPIO pins as outputs
// in a sequence, indefinitely.

console.log("Test external GPIOs as outputs...\n");

console.log('Wire an external LED to each output in turn!');

var board = require('board');
var gpio = require('gpio');

var testpins;
if (board.name == 'arduino_101') {
    // expected results: works with IO2, IO4, IO7-8, IO10-13; others have
    //   incompatible pinmux settings, I think
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
}
else if (board.name == 'frdm_k64f') {
    // expected results: all pins but D8 will blink (not sure why not D8)
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
}
else if (board.name == 'nucleo_f411re') {
    // expected results: pins D2-D15 will blink; onboard LED 'LD2' blinks in
    //   sync with D13; D0/D1 are set as serial so can't be used
    // NOTE: D0 (GPIOA.3) and D1 (GPIOA.2) must be left out or they will
    //   corrupt serial output
    testpins = ['GPIOA.10', 'GPIOB.3', 'GPIOB.5', 'GPIOB.4', 'GPIOB.10',
                'GPIOA.8', 'GPIOA.9', 'GPIOC.7', 'GPIOB.6', 'GPIOA.7',
                'GPIOA.6', 'GPIOA.5', 'GPIOB.9', 'GPIOB.8'];
}

// open all pins for output
var pincount = testpins.length;
var gpios = [];
for (var i = 0; i < pincount; i++) {
    gpios[i] = gpio.open({pin: testpins[i]});
}

// toggle all pins off in sequence, then on in sequence, and so on
var tick = 75, toggle = 0;
var index = 0;
setInterval(function () {
    gpios[index++].write(toggle);
    if (index >= pincount) {
        toggle = 1 - toggle;
        index = 0;
    }
}, tick);
