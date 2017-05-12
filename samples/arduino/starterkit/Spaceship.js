// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino Starter Kit's Spaceship example
//   - Toggles LEDs in while a button is pressed
//   - This "easy" version does not quite match the original behavior, since
//       we only check the button state every 250ms. So the lasers may take up
//       to 250ms to engage, which could mean the difference between life and
//       death. See Spaceship2.js for a more accurate version.

// Hardware Requirements:
//   - Arduino 101
//   - Three LEDs and three 220-ohm resistors
//   - One pushbutton and one 10K-ohm pulldown resistor
// Wiring:
//   Use the same wiring as in the Starter Kit Spaceship sample, except:
//     - Instead of IO3/IO4/IO5 for output LEDs use IO4/IO7/IO8

var gpio = require('gpio');

// set up the GPIO pins
var button = gpio.open({pin: 2, mode: 'in'});
var led1 = gpio.open(4);
var led2 = gpio.open(7);
var led3 = gpio.open(8);

var btnstate = false;
var toggle = 0;

// check button state every 250ms (1/4 second)
setInterval(function () {
    if (button.read()) {
        // the button is being pressed
        if (!btnstate) {
            // it wasn't pressed last time we checked
            btnstate = true;
            toggle = 0;
            led1.write(0);
        }

        toggle = 1 - toggle;
        led2.write(1 - toggle);
        led3.write(toggle);
    }
    else {
        // the button is not being pressed
        btnstate = false;
        led1.write(1);
        led2.write(0);
        led3.write(0);
    }
}, 250);
