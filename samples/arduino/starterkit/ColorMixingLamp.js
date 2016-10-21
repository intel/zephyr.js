// Copyright (c) 2016, Intel Corporation.

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
var pins = require('arduino101_pins');

// set up the PWM pins
var redLED = pwm.open({
    channel: pins.IO3
});
var greenLED = pwm.open({
    channel: pins.IO5
});
var blueLED = pwm.open({
    channel: pins.IO6
});

// set up the AIO pins
var redSensor = aio.open({
    device: 0,
    pin: pins.A0
});
var greenSensor = aio.open({
    device: 0,
    pin: pins.A1
});
var blueSensor = aio.open({
    device: 0,
    pin: pins.A2
});

// initialize the period of the PWMs to 256 hardware cycles
redLED.setPeriodCycles(256);
greenLED.setPeriodCycles(256);
blueLED.setPeriodCycles(256);

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

    redLED.setPulseWidthCycles(redValue);
    greenLED.setPulseWidthCycles(greenValue);
    blueLED.setPulseWidthCycles(blueValue);

    // FIXME: Currently, ZJS only reads analog pins once per second, so there's
    // no point in checking more often. This should be fixed soon.
}, 1000);
