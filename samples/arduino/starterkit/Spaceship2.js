// Copyright (c) 2016-2017, Intel Corporation.

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

// set up the GPIO pins
var button = gpio.open({pin: 2, mode: 'in', edge: 'rising'});
var led1 = gpio.open(4);
var led2 = gpio.open(7);
var led3 = gpio.open(8);

// turn green LED on initially
led1.write(1);

var timer = null;

button.onchange = function () {
    if (timer) {
        // lasers still on, don't bother
        return;
    }

    // button has been pressed
    var toggle = 0;
    led1.write(0);
    led2.write(0);
    led3.write(1);

    // start timer to toggle LED states every 250ms (1/4 second)
    timer = setInterval(function () {
        if (!button.read()) {
            led1.write(1);
            led2.write(0);
            led3.write(0);

            // remove the timer event
            clearInterval(timer);
            timer = null;
            return;
        }

        toggle = 1 - toggle;
        led2.write(toggle);
        led3.write(1 - toggle);
    }, 250);
}
