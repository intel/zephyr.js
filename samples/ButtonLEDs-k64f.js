// Copyright (c) 2016, Intel Corporation.

// Test code for FRDM-K64F that uses the onboard RGB LED for output, and
// expects a button or similar input connected to D8. Blinks the LEDs on and
// off together, but when the button is pressed turns one on and one off until
// the next timer expires.

print("GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");
var pins = require("k64f_pins");

var pinA = null;
var pinB = null;

gpio.open({ pin: pins.LEDR }).then(function(pin) {
    pinA = pin;
});

gpio.open({ pin: pins.LEDB }).then(function(pin) {
    pinB = pin;
});

gpio.open({ pin: pins.D7, direction: 'in', edge: 'rising' }).then(function(pin) {
    print("Input pin successfully opened!");

    // tick is the delay between blinks
    var tick = 1000, toggle = false;

    setInterval(function () {
        toggle = !toggle;
        pin.read();
        pinA.write(toggle);
        pinB.write(toggle);
    }, tick);

    pin.onchange = function(event) {
        print("In pin onchange callback!");
        pinA.write(true);
        pinB.write(false);
    };
}).catch(function(error) {
    print("Error opening GPIO pin");
});
