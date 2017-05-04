// Copyright (c) 2016-2017, Intel Corporation.

// Test Arduino101Pins API

var pins = require('arduino101_pins');
var gpio = require('gpio');
var pwm = require('pwm');
var aio = require('aio');
var assert = require("Assert.js");

// Check pins defined and typeof Number
function checkDefined(name) {
    assert(name in pins && typeof pins[name] == "number",
          "Arduino101Pins: " + name + " defined");
}

// GPIO Pins
var GPIOPins = ["IO2", "IO3", "IO4", "IO5",
                "IO6", "IO7", "IO8", "IO9",
                "IO10", "IO11", "IO12", "IO13"];
for(var i = 0; i < GPIOPins.length; i++) {
    var pinName = GPIOPins[i];

    checkDefined(pinName);

    // IO6 and IO9 are defined but unusable as GPIOs currently
    if (pinName == "IO6" || pinName == "IO9") continue;

    var pinA = gpio.open({ pin: pins[pinName], direction: "in" });
    var pinAValue1 = pinA.read();
    var ApullValue = (pinAValue1) ? " weak pull-up" : " weak pull-down";

    assert(typeof pinAValue1 == "boolean",
           "Arduino101Pins: " + pinName + ApullValue + " on input");

    pinA.write(false);
    var pinAValue2 = pinA.read();

    assert(typeof pinAValue2 == "boolean" && pinAValue1 == pinAValue2,
           "Arduino101Pins: " + pinName + " input");

    var pinB = gpio.open({ pin: pins[pinName], direction: "out" });
    var pinBValue1 = pinB.read();
    var BpullValue = (pinBValue1) ? " weak pull-up" : " weak pull-down";

    assert(typeof pinBValue1 == "boolean",
           "Arduino101Pins: " + pinName + BpullValue + " on output");

    pinB.write(true);
    var pinBValue2 = pinB.read();

    if (pinName == "IO3" || pinName == "IO5") {
        // IO3 and IO5 can be used as GPIO inputs but not outputs currently
        assert(typeof pinBValue2 == "boolean" && pinBValue2 == pinBValue1,
              "Arduino101Pins: " + pinName + " not output");
    } else {
        assert(typeof pinBValue2 == "boolean" && pinBValue2 != pinBValue1,
              "Arduino101Pins: " + pinName + " output");
    }
}

// LEDs
var LEDs = [["LED0", true],
            ["LED1", true],
            ["LED2", false]];
for(var i = 0; i < LEDs.length; i++) {
    var pinName = LEDs[i][0];

    checkDefined(pinName);

    // activeLow
    var lowFlag = LEDs[i][1];
    var pin = gpio.open({pin: pins[pinName], activeLow: lowFlag});
    var pinValue = pin.read();
    var lowStr = lowFlag ? "high" : "low";
    assert(pinValue,
          "Arduino101Pins: " + pinName + " active " + lowStr);

    pin.write(!pinValue)
    assert(pin.read() != pinValue,
          "Arduino101Pins: " + pinName + " output");

    if (pinName == "LED2") {
        pinValue = pin.read();
        var io13 = gpio.open({pin: pins.IO13, activeLow: lowFlag});
        io13.write(!pinValue);
        assert(pin.read() != pinValue,
            "Arduino101Pins: " + pinName + " displays current state of IO13");
    }
}

// PWM Pins
var PWMPins = ["PWM0", "PWM1", "PWM2", "PWM3"];
for(var i = 0; i < PWMPins.length; i++) {
    var pinName = PWMPins[i];

    checkDefined(pinName);

    var pin = pwm.open({channel: pins[pinName]});
    assert(pin != null && typeof pin == "object",
           "Arduino101Pins: " + pinName + " open");
}

// AIO Pins
var AIOPins = ["A0", "A1", "A2", "A3", "A4", "A5"];
for(var i = 0; i < AIOPins.length; i++) {
    var pinName = AIOPins[i];

    checkDefined(pinName);

    var pin = aio.open({ device: 0, pin: pins[pinName] });
    var pinValue = pin.read();
    assert(pinValue >= 0 && pinValue <= 4095,
           "Arduino101Pins: " + pinName + " digital value");
}

assert.result();
