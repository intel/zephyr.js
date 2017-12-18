// Copyright (c) 2016-2017, Intel Corporation.

// Test Arduino101Pins API

console.log("Test Arduino 101 pins API");

var gpio = require('gpio');
var pwm = require('pwm');
var aio = require('aio');
var assert = require('Assert.js');

// GPIO Pins
var GPIOPins = ["IO2", "IO3", "IO4", "IO5",
                "IO6", "IO7", "IO8", "IO9",
                "IO10", "IO11", "IO12", "IO13",
                "PWM0", 1024, "GPIO_9.1"];
for (var i = 0; i < GPIOPins.length; i++) {
    var pinName = GPIOPins[i];

    // IO6 and IO9 are defined but unusable as GPIOs currently
    if (pinName === "IO6" || pinName === "IO9") continue;

    if (i >= 12) {
        assert.throws(function() {
            var pinA = gpio.open({pin: pinName, mode: "in"});
        }, "GPIO Pins: " + pinName + " not as GPIO pin");

        continue
    }

    var pinA = gpio.open({pin: pinName, mode: "in"});
    assert(typeof pinA === "object" && pinA !== null,
           "GPIO Pins: " + pinName + " is defined");

    var pinAValue1 = pinA.read();
    var ApullValue = (pinAValue1) ? "weak pull-up" : "weak pull-down";

    assert(typeof pinAValue1 === "number",
           "GPIO Pins: " + pinName + " has " + ApullValue + " on input pin");

    pinA.write(0);
    var pinAValue2 = pinA.read();

    assert(typeof pinAValue2 === "number" && pinAValue1 === pinAValue2,
           "GPIO Pins: " + pinName + " as input pin");

    pinA.close();

    var pinB = gpio.open({pin: pinName, mode: "out"});
    var pinBValue1 = pinB.read();
    var BpullValue = (pinBValue1) ? "weak pull-up" : "weak pull-down";

    assert(typeof pinBValue1 === "number",
           "GPIO Pins: " + pinName + " has " + BpullValue + " on output pin");

    pinB.write(1);
    var pinBValue2 = pinB.read();

    if (pinName === "IO3" || pinName === "IO5") {
        // IO3 and IO5 can be used as GPIO inputs but not outputs currently
        assert(typeof pinBValue2 === "number" && pinBValue2 === pinBValue1,
              "GPIO Pins: " + pinName + " not as output pin");
    } else {
        assert(typeof pinBValue2 === "number" && pinBValue2 !== pinBValue1,
              "GPIO Pins: " + pinName + " as output pin");
    }

    pinB.close();
}

// LEDs
var LEDs = [
    ["LED0", true],
    ["LED1", true],
    ["LED2", false],
    ["PWM0", true],
    [1024, false],
    ["GPIO_9.1", true]
];
for (var i = 0; i < LEDs.length; i++) {
    var pinName = LEDs[i][0];
    var lowFlag = LEDs[i][1];

    if (i >= 3) {
        assert.throws(function() {
            var pin = gpio.open({pin: pinName, activeLow: lowFlag});
        }, "LED Pins: " + pinName + " not as LED pin");

        continue
    }

    var pin = gpio.open({pin: pinName, activeLow: lowFlag});
    assert(typeof pin === "object" && pin !== null,
           "LED Pins: " + pinName + " is defined");

    var pinValue = pin.read();
    var lowStr = lowFlag ? "high" : "low";
    assert(pinValue === 1,
          "LED Pins: " + pinName + " active " + lowStr);

    pin.write(1 - pinValue);
    assert(pin.read() !== pinValue,
          "LED Pins: " + pinName + " as output pin");

    if (pinName === "LED2") {
        pinValue = pin.read();

        var io13 = gpio.open({pin: "IO13", activeLow: lowFlag});
        io13.write(1 - pinValue);

        assert(pin.read() !== pinValue,
            "LED Pins: " + pinName + " displays current state of IO13");

        io13.close();
    }

    pin.close();
}

// PWM Pins
var PWMPins = ["PWM0", "PWM1", "PWM2", "PWM3", "1024", 1024, "PWM1024"];
for (var i = 0; i < PWMPins.length; i++) {
    var pinName = PWMPins[i];

    if (i >= 4) {
        assert.throws(function() {
            var pin = pwm.open(pinName);
        }, "PWM Pins: " + pinName + " not as PWM pin");

        continue
    }

    var pin = pwm.open(pinName);
    assert(pin !== null && typeof pin === "object",
           "PWM Pins: " + pinName + " is defined");
}

// AIO Pins
var AIOPins = ["A0", "A1", "A2", "A3", "A4", "A5", "A1024", "AD1024", 1024];
for (var i = 0; i < AIOPins.length; i++) {
    var pinName = AIOPins[i];

    if (i >= 6) {
        assert.throws(function() {
            var pin = aio.open({pin: pinName});
        }, "AIO Pins: " + pinName + " not as AIO pin");

        continue
    }

    var pin = aio.open({pin: pinName});
    assert(pin !== null && typeof pin === "object",
           "AIO Pins: " + pinName + " is defined");

    var pinValue = pin.read();
    assert(pinValue >= 0 && pinValue <= 4095,
           "AIO Pins: " + pinName + " digital value");
}

assert.result();
