// Copyright (c) 2016, Intel Corporation.

// Test code for FRDM-K64F that uses the onboard RGB LED for output, and
// onboard input button SW2. Blinks the LEDs on and off together, but when the
// button is pressed the red light turns off and blue light turns on, until the
// next timer expires.

print("GPIO test with two LEDs and a button...");

// import gpio module
var gpio = require("gpio");
var pins = require("k64f_pins");

var pinA = null;
var pinB = null;

// D0 - D13 will also work as these output pins, if you hook up an LED
gpio.open({ pin: pins.LEDR }).then(function(pin) {
    pinA = pin;
});

gpio.open({ pin: pins.LEDB }).then(function(pin) {
    pinB = pin;
});

// D0 - D15 will also work as this input pin, if you hook up a button
gpio.open({ pin: pins.SW2, direction: 'in',
            edge: 'falling' }).then(function(pin) {
    // tick is the delay between blinks
    var tick = 1000, toggle = false;

    setInterval(function () {
        toggle = !toggle;
        pin.read();
        pinA.write(toggle);
        pinB.write(toggle);
    }, tick);

    pin.onchange = function(event) {
        pinA.write(true);
        pinB.write(false);
    };
}).catch(function(error) {
    print("Error opening GPIO pin");
});
