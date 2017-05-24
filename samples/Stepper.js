// Copyright (c) 2017, Intel Corporation.

// Sample code for Arduino 101 w/ Grove Stepper Motor: 28BYJ-48, with ULM2003 control board

console.log("Stepper Motor sample...");

var gpio = require('gpio');

//A+: D2/IO2
//A-: D4/IO4
//B+: D7/IO7
//B-: D8/IO8

var pinA1 = gpio.open({ pin: 'IO2', mode: 'out', activeLow: false });
var pinA2 = gpio.open({ pin: 'IO4', mode: 'out', activeLow: false });
var pinB1 = gpio.open({ pin: 'IO7', mode: 'out', activeLow: false });
var pinB2 = gpio.open({ pin: 'IO8', mode: 'out', activeLow: false });

var dir = 0;

// Appropriate sleep amount to ensure GPIO line is energized long enough
// for the stepper motor to step - found experimentally
var sleepDelay = 10;

// Super hacky sleep emulation - we leave a multiply operation in to take more time
function sleep(delay) {
    for (var i = 0; i < delay * 4; ){
        i++;
    }
}

// While the 28BYJ-48 specifies a half-step 8-phase pattern, experimentation shows
// that a full-step incremental pattern works better in ZJS
function stepIt(A1, A2, B1, B2) {
    if (dir) {
        pinA1.write(1);
        sleep(sleepDelay);
        pinA1.write(0);

        pinA2.write(1);
        sleep(sleepDelay);
        pinA2.write(0);

        pinB1.write(1);
        sleep(sleepDelay);
        pinB1.write(0);

        pinB2.write(1);
        sleep(sleepDelay);
        pinB2.write(0);
    } else {
        pinB2.write(1);
        sleep(sleepDelay);
        pinB2.write(0);

        pinB1.write(1);
        sleep(sleepDelay);
        pinB1.write(0);

        pinA2.write(1);
        sleep(sleepDelay);
        pinA2.write(0);

        pinA1.write(1);
        sleep(sleepDelay);
        pinA1.write(0);
    }
}

function reverseIt () {
    dir = 1 - dir;
}

//Start stepping as fast as we can
setInterval(stepIt, 1);

//Reverse direction every 5 seconds
setInterval(reverseIt, 5000);
