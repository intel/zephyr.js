// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino Starter Kit's Love-o-Meter example
//   - Lights up 0-3 LEDs in response to temperature sensor reading

// Hardware Requirements:
//   - Arduino 101
//   - Three LEDs and three 220-ohm resistors
//   - One TMP36 temperature sensor
// Wiring:
//   Use the same wiring as in the Starter Kit Love-o-Meter sample, except:
//     - Instead of IO3/IO4/IO5 for output LEDs use IO4/IO7/IO8

var gpio = require('gpio');
var aio = require('aio');
var pins = require('arduino101_pins');

// set up the GPIO pins
var sensor = aio.open({
    device: 0,
    pin: pins.A0
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

var baselineTemp = 20.0;

var leds = [led1, led2, led3];

// initialize each LED to off (false)
for (var id in leds) {
    leds[id].write(false);
}

function clipNum(num) {
    // HACK: work around current lack of Math.round
    return (num * 1000 | 0) / 1000.0
}

var lastValue;

setInterval(function () {
    // start a line to be sent to the serial console
    var line = '';

    // read the sensor value and add it to the output
    var sensorValue = sensor.read();
    line += 'Sensor Value: ' + sensorValue;

    // only continue if the value has changed since last time
    if (sensorValue == lastValue)
        return;
    lastValue = sensorValue;

    // convert the value to voltage and add it to the output
    // NOTE: The Arduino 101 has 12-bit ADC resolution, so the range is 0-4095,
    //   unlike the Arduino Uno's 10-bit 0-1023 range.
    var voltage = sensorValue / 4095.0 * 3.3;
    line += ', Volts: ' + clipNum(voltage);

    // convert the voltage to temperature and add it to the output
    var temperature = (voltage - 0.5) * 100;
    line += ', degrees C: ' + clipNum(temperature);

    var fahrenheit = (temperature / 5 * 9) + 32;
    line += ', degrees F: ' + clipNum(fahrenheit);

    // send the output to the serial console
    console.log(line);

    // Note: The original code seems to have a bug in that it doesn't handle
    //   temperatures between the baseline and baseline + 2, so this is
    //   probably what they intended.
    if (temperature < baselineTemp) {
        led1.write(false);
        led2.write(false);
        led3.write(false);
    }
    else if (temperature < baselineTemp + 2) {
        led1.write(true);
        led2.write(false);
        led3.write(false);
    }
    else if (temperature < baselineTemp + 4) {
        led1.write(true);
        led2.write(true);
        led3.write(false);
    }
    else {
        led1.write(true);
        led2.write(true);
        led3.write(true);
    }

// This interval of 10ms is the minimum we can currently handle on Arduino 101,
//   which is a bit different from the original example code, which had a delay
//   of 1ms.
}, 10);
