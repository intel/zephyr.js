// Copyright (c) 2016, Intel Corporation.

// Test Arduino101Pins API

var pins = require('arduino101_pins');
var gpio = require('gpio');
var pwm = require('pwm');
var aio = require('aio');

var total = 0;
var passed = 0;

function assert(actual, description) {
    total += 1;

    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    console.log(label + " - " + description);
}

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (err) {
        threw = true;
    }
    assert(threw, description);
}

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

    var pin = gpio.open({ pin: pins[pinName] });
    var pinValue = pin.read();
    assert(typeof pinValue == "boolean",
           "Arduino101Pins: " + pinName + " input");

    pin.write(!pinValue);
    if (pinName == "IO3" || pinName == "IO5") {
        // IO3 and IO5 can be used as GPIO inputs but not outputs currently
        assert(pin.read() == pinValue,
              "Arduino101Pins: " + pinName + " output");
    } else {
        assert(pin.read() != pinValue,
              "Arduino101Pins: " + pinName + " output");
    }
}

// LEDs
var LEDs = [["LED0", false],
            ["LED1", true ],
            ["LED2", true ]];
for(var i = 0; i < LEDs.length; i++) {
    var pinName = LEDs[i][0];

    checkDefined(pinName);

    // activeLow
    var lowFlag = LEDs[i][1];
    var pin = gpio.open({ pin: pins[pinName], activeLow: lowFlag });
    var pinValue = pin.read();
    var lowStr = lowFlag ? "high" : "low";
    assert(pinValue,
          "Arduino101Pins: " + pinName + " active " + lowStr);

    pin.write(!pinValue)
    assert(pin.read() != pinValue,
          "Arduino101Pins: " + pinName + " output");

    if (pinName == "LED0") {
        pinValue = pin.read();
        var io13 = gpio.open({ pin: pins.IO13, activeLow: lowFlag });
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

console.log("TOTAL: " + passed + " of " + total + " passed");
