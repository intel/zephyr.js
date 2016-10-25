// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino Starter Kit's Spaceship example
//   - Toggles LEDs in response to a button press
//   - This "advanced" version matches the original behavior, by reacting
//       immediately to a change in button state, at the expense of the code
//       being a little more complex. See Spaceship.js for a simpler version.

// Hardware Requirements:
//   - Arduino 101
//   - Three LEDs and three 220-ohm resistors
//   - One pushbutton and one 10K-ohm pulldown resistor
// Wiring:
//   Use the same wiring as in the Starter Kit Spaceship sample, except:
//     - Instead of IO3/IO4/IO5 for output LEDs use IO4/IO7/IO8

var gpio = require('gpio');
var pins = require('arduino101_pins');

// set up the GPIO pins
var button = gpio.open({
    pin: pins.IO2,
    direction: 'in',
    edge: 'any'
});
var led1 = gpio.open({
    pin: pins.IO4,
    direction: 'out'
});
var led2 = gpio.open({
    pin: pins.IO7,
    direction: 'out'
});
var led3 = gpio.open({
    pin: pins.IO8,
    direction: 'out'
});

var btnstate = false;

// turn green LED on initially
led1.write(true);

var timer;
var toggle;

button.onchange = function (event) {
    if (event.value) {
        // button has been pressed
        toggle = false;
        led1.write(false);
        led2.write(false);
        led3.write(true);

        // start timer to toggle LED states every 250ms (1/4 second)
        timer = setInterval(function () {
            toggle = !toggle;
            led2.write(toggle);
            led3.write(!toggle);
        }, 250);
    }
    else {
        // button has been released
        led1.write(true);
        led2.write(false);
        led3.write(false);

        // remove the timer event
        clearInterval(timer);
    }
}
