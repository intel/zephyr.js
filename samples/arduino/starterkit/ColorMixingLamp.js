// Copyright (c) 2016-2017, Intel Corporation.

// Reimplementation of Arduino Starter Kit's ColorMixingLamp example
//   - Lights up an RGB LED based on the ambient light color detected by three
//       filtered photosresistors

// Hardware Requirements:
//   - Arduino 101
//   - One RGB LED and three 220-ohm resistors
//   - Three photoresistors, R/G/B color filters, and three 10K-ohm resistors
// Wiring:
//   Use the same wiring as in the Starter Kit ColorMixingLamp sample, except:
//     - Instead of wiring the green LED pin to IO9, wire it to IO5
//     - Instead of wiring the red LED pin to IO11, wire it to IO3
//     - Instead of wiring the blue LED pin to IO10, wire it to IO6

var pwm = require('pwm');
var aio = require('aio');

// set up the PWM pins
var redLED = pwm.open('IO3');
var greenLED = pwm.open('IO5');
var blueLED = pwm.open('IO6');

// set up the AIO pins
var redSensor = aio.open('A0');
var greenSensor = aio.open('A1');
var blueSensor = aio.open('A2');

setInterval(function () {
    var redValue = redSensor.read();
    var greenValue = greenSensor.read();
    var blueValue = blueSensor.read();

    console.log("Raw Sensor Values: " +
                "\tRed: " + redValue +
                "\tGreen: " + greenValue +
                "\tBlue: " + blueValue);

    // NOTE: The Arduino has 12-bit ADC resolution (0-4095) vs. the Arduino
    //   Uno's 10-bit resolution (0-1023), so we divide by 16 instead of 4 to
    //   get the values down to the 0-255 range.
    redValue = redValue / 16;
    greenValue = greenValue / 16;
    blueValue = blueValue / 16;

    console.log("Mapped Sensor Values: " +
                "\tRed: " + redValue +
                "\tGreen: " + greenValue +
                "\tBlue: " + blueValue);

    redLED.setCycles(256, redValue);
    greenLED.setCycles(256, greenValue);
    blueLED.setCycles(256, blueValue);

    // FIXME: Currently, ZJS only reads analog pins once per second, so there's
    // no point in checking more often. This should be fixed soon.
}, 1000);
