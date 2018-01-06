// Copyright (c) 2016-2018, Intel Corporation.

console.log("Test FRDM-K64F pins APIs");

var gpio = require('gpio');
var assert = require("Assert.js");

// GPIO Pins
var GPIOPins = ["D0", "D1", "D2", "D3", "D4", "D5",
                "D6", "D7", "D8", "D9", "D10", "D11",
                "D12", "D13", "D14", "D15", "PWM0",
                1024, "1024", "GPIO_9.1"];

for (var i = 0; i < GPIOPins.length; i++) {
    var pinName = GPIOPins[i];

    // D8 are defined but unusable as GPIOs currently
    if (pinName === "D8") continue;

    if (i >= 16) {
        assert.throws(function() {
            var pinA = gpio.open({pin: pinName, mode: "in"});
        }, "GPIO Pins: " + pinName + " not as GPIO pin");

        continue
    }

    // GPIOPins as input pins
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

    // GPIOPins as output pins
    var pinB = gpio.open({pin: pinName, mode: "out"});
    var pinBValue1 = pinB.read();
    var BpullValue = (pinBValue1) ? "weak pull-up" : "weak pull-down";

    assert(typeof pinBValue1 === "number",
           "GPIO Pins: " + pinName + " has " + BpullValue + " on output pin");

    pinB.write(1);
    var pinBValue2 = pinB.read();

    assert(typeof pinBValue2 === "number" && pinBValue2 !== pinBValue1,
          "GPIO Pins: " + pinName + " as output pin");
}

// LEDs
var LEDs = [
    ["LED0", false],
    ["LED1", false],
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
    assert(pinValue === 0,
           "LED Pins: " + pinName + " active " + lowStr);

    pin.write(1 - pinValue);
    assert(pin.read() !== pinValue,
           "LED Pins: " + pinName + " as output pin");

    pin.close();
}

// Switches
var SWs = ["SW2", "SW3", "PWM0", 1024, "GPIO_9.1"];
for (var i = 0; i < SWs.length; i++) {
    var pinName = SWs[i];

    if (i >= 2) {
        assert.throws(function() {
            var pin = gpio.open({pin: pinName, mode: "in"});
        }, "SW Pins: " + pinName + " not as SW pin");

        continue
    }

    var pin = gpio.open({pin: pinName, mode: "in"});
    assert(typeof pin === "object" && pin !== null,
           "SW Pins: " + pinName + " is defined");

    var SWValue = pin.read();
    assert(typeof SWValue === "number",
           "SW Pins: "+ pinName + " as input pin");

    pin.write(1 - SWValue);
    assert(pin.read() === SWValue,
           "SW Pins: " + pinName + " not as output pin");

    pin.close();
}

// PWM Pins
// Not support at now
/*
var PWMPins = ["PWM0", "PWM1", "PWM2", "PWM3",
               "PWM4", "PWM5", "PWM6", "PWM7",
               "PWM8", "PWM9", "PWM10", "PWM11",
               "1024", 1024, "PWM1024"];

for (var i = 0; i < PWMPins.length; i++) {
    var pinName = PWMPins[i];

    if (i >= 12) {
        assert.throws(function() {
            var pin = pwm.open(pinName);
        }, "PWM Pins: " + pinName + " not as PWM pin");

        continue
    }

    var pin = pwm.open(pinName);
    assert(pin !== null && typeof pin === "object",
           "PWM Pins: " + pinName + " is defined");
}
*/
// AIO Pins
var aio = require('aio');

var AIOPins = ["A0", "A1", "A2", "A3", "A4", "A5",
               "A6", "A7", "A1024", "AD1024", 1024];

for (var i = 0; i < AIOPins.length; i++) {
    var pinName = AIOPins[i];

    if (i >= 8) {
        assert.throws(function() {
            var pin = aio.open({pin: pinName});
        }, "AIO Pins: " + pinName + " not as AIO pin");

        continue
    }

    if (pinName === "A4" || pinName === "A5") continue;

    var pin = aio.open({pin: pinName});
    assert(pin !== null && typeof pin === "object",
           "AIO Pins: " + pinName + " is defined");

    var pinValue = pin.read();
    assert(pinValue >= 0 && pinValue <= 4095,
           "AIO Pins: " + pinName + " digital value");
}

assert.result();
