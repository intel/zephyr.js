// Copyright (c) 2016, Intel Corporation.

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
var pins = require('arduino101_pins');

// set up the GPIO pins
var button = gpio.open({
    pin: pins.IO2,
    direction: 'in'
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
var toggle = false;

// check button state every 250ms (1/4 second)
setInterval(function () {
    if (button.read()) {
        // the button is being pressed
        if (!btnstate) {
            // it wasn't pressed last time we checked
            btnstate = true;
            toggle = false;
            led1.write(false);
        }

        toggle = !toggle;
        led2.write(!toggle);
        led3.write(toggle);
    }
    else {
        // the button is not being pressed
        btnstate = false;
        led1.write(true);
        led2.write(false);
        led3.write(false);
    }
}, 250);
