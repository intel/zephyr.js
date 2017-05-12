// Copyright (c) 2016-2017, Intel Corporation.

console.log("Test K64f_pins APIs");

var pins = require('k64f_pins');
var gpio = require('gpio');
var assert = require("Assert.js");

// Check pins defined and typeof Number
function checkDefined(name) {
    assert(name in pins && typeof pins[name] == "number",
          "K64f_pins: " + name + " defined");
}

// GPIO Pins
var GPIOPins = ["D0", "D1", "D2", "D3", "D4", "D5",
                "D6", "D7", "D8", "D9", "D10", "D11",
                "D12", "D13", "D14", "D15"];

for (var i = 0; i < GPIOPins.length; i++) {
    var pinName = GPIOPins[i];

    checkDefined(pinName);

    // D8 are defined but unusable as GPIOs currently
    if (pinName == "D8") continue;

    // GPIOPins as input pins
    var pinA = gpio.open({pin: pinName, mode: "in"});
    var pinAValue1 = pinA.read();
    var ApullValue = (pinAValue1) ? " weak pull-up" : " weak pull-down";

    assert(typeof pinAValue1 == "number",
           "K64f_pins: " + pinName + ApullValue + " on input");

    pinA.write(0);
    var pinAValue2 = pinA.read();

    assert(typeof pinAValue2 == "number" && pinAValue1 == pinAValue2,
           "K64f_pins: " + pinName + " input");

    // GPIOPins as output pins
    var pinB = gpio.open({pin: pinName, mode: "out"});
    var pinBValue1 = pinB.read();
    var BpullValue = (pinBValue1) ? " weak pull-up" : " weak pull-down";

    assert(typeof pinBValue1 == "number",
           "K64f_pins: " + pinName + BpullValue + " on output");

    pinB.write(1);
    var pinBValue2 = pinB.read();

    assert(typeof pinBValue2 == "number" && pinBValue2 != pinBValue1,
          "K64f_pins: " + pinName + " output");
}

// LEDs
var LEDs = [
    ["LEDR", false],
    ["LEDG", true],
    ["LEDB", false]
];

for (var i = 0; i < LEDs.length; i++) {
    var pinName = LEDs[i][0];

    checkDefined(pinName);

    // activeLow
    var lowFlag = LEDs[i][1];
    var pin = gpio.open({pin: pinName, activeLow: lowFlag});

    var pinValue = pin.read();
    assert(pinValue == lowFlag, "K64f_pins: " + pinName + " active high");

    pin.write(1 - pinValue);
    assert(pin.read() != pinValue, "K64f_pins: " + pinName + " output");
}

// Switches
checkDefined("SW2");
checkDefined("SW3");

var SW2 = gpio.open({pin: "SW2", mode: "in"});

var SWValue = SW2.read();
assert(typeof SWValue == "number", "K64f_pins: SW2 input");

SW2.write(1 - SWValue);
assert(SW2.read() == SWValue, "K64f_pins: SW2 not output");

// PWM Pins
var PWMPins = ["PWM0", "PWM1", "PWM2", "PWM3",
               "PWM4", "PWM5", "PWM6", "PWM7",
               "PWM8", "PWM9"];

for (var i = 0; i < PWMPins.length; i++) {
    var pinName = PWMPins[i];

    checkDefined(pinName);
}

// AIO Pins
var AIOPins = ["A0", "A1", "A2", "A3", "A4", "A5"];

for (var i = 0; i < AIOPins.length; i++) {
    var pinName = AIOPins[i];

    checkDefined(pinName);
}

assert.result();
