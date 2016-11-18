// Copyright (c) 2016, Intel Corporation.

console.log("Test K64f_pins APIs");

var pins = require('k64f_pins');
var gpio = require('gpio');

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
          "K64f_pins: " + name + " defined");
}

// GPIO Pins
var GPIOPins = ["D0", "D1", "D2", "D3", "D4", "D5",
                "D6", "D7", "D8", "D9", "D10", "D11",
                "D12", "D13", "D14", "D15"];

for (var i = 0; i < GPIOPins.length; i++) {
    var pinName = GPIOPins[i];

    checkDefined(pinName);

    // D3, D5, D8 are defined but unusable as GPIOs currently
    if (pinName == "D3" || pinName == "D5" || pinName == "D8") continue;

    // GPIOPins as input pins
    var pin = gpio.open({ pin: pins[pinName], direction: "in" });
    var pinValue = pin.read();
    assert(typeof pinValue == "boolean", "K64f_pins: " + pinName + " input");

    // GPIOPins as output pins
    pin = gpio.open({ pin: pins[pinName], direction: "out" });
    pinValue = pin.read();
    pin.write(!pinValue);

    // D14, D15 can be used as GPIO inputs but not outputs currently
    if (pinName == "D14" || pinName == "D15") {
        assert(pin.read() == pinValue, "K64f_pins: " + pinName + " not output");
    } else {
        assert(pin.read() != pinValue, "K64f_pins: " + pinName + " output");
    }
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
    var pin = gpio.open({ pin: pins[pinName], activeLow: lowFlag });

    var pinValue = pin.read();
    assert(pinValue == lowFlag, "K64f_pins: " + pinName + " active high");

    pin.write(!pinValue);
    assert(pin.read() != pinValue, "K64f_pins: " + pinName + " output");
}

// Switches
checkDefined("SW2");
checkDefined("SW3");

var SW2 = gpio.open({ pin: pins.SW2, direction: "in" });

var SWValue = SW2.read();
assert(typeof SWValue == "boolean", "K64f_pins: SW2 input");

SW2.write(!SWValue);
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

console.log("TOTAL: " + passed + " of " + total + " passed");
