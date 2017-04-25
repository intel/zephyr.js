// Copyright (c) 2016-2017, Intel Corporation.

// Testing GPIO APIs

var gpio = require("gpio");
var pins = require("arduino101_pins");
var assert = require("Assert.js");

if (gpio.mock) {
    gpio.wire(gpio.open({pin: pins.IO7}), gpio.open({pin: pins.IO8}));
}
else {
    // Pre-conditions
    console.log("Hardware setup: Wire IO7 to IO8!");
}

var pinA, pinB, aValue, bValue;

// test GPIOPin onchange
var changes = [
    ["falling", 1, true],
    ["rising", 1, false],
    ["any", 1, false]
];
var mark = 0;

var edgeInterval = setInterval(function () {
    var count = 0;
    pinA = gpio.open({pin: pins.IO7});
    pinA.write(changes[mark][2]);
    pinB = gpio.open({
        pin: pins.IO8,
        direction: "in",
        edge: changes[mark][0]
    });

    pinB.onchange = function () {
        count++;
        pinB.close();
    };

    pinA.write(!changes[mark][2]);

    setTimeout(function () {
        assert(count == changes[mark][1],
               "gpiopin: onchange with edge '" + changes[mark][0] + "'");

        if (changes[mark][0] == "any") {
            // test GPIOPin close
            pinA.write(!changes[mark][2]);
            assert(count == changes[mark][1], "gpiopin: close onchange");
        }

        mark = mark + 1;

        if (mark == 3) {
            assert.result();
            clearInterval(edgeInterval);
        }
    }, 100);
}, 200);

// test GPIO open
pinA = gpio.open({pin: pins.IO7});
pinB = gpio.open({pin: pins.IO8, direction: "in"});

assert(pinA != null && typeof pinA == "object",
      "open: defined pin and default as 'out' direction");

assert(pinB != null && typeof pinB == "object",
      "open: defined pin with direction 'in'");

assert.throws(function () {
    gpio.open({pin: 1024});
}, "open: invalid pin");

// test GPIOPin read and write
pinA.write(true);
bValue = pinB.read();
assert(bValue, "gpiopin: write and read");

// reading an output pin may not be valid; this is probably a bad test
aValue = pinA.read();
assert(aValue, "gpiopin: read output pin");

pinB.write(false);
bValue = pinB.read();
assert(bValue, "gpiopin: write input pin");

assert.throws(function () {
    pinA.write(1);
}, "gpiopin: write invalid argument");

// test activeLow
pinB = gpio.open({pin: pins.IO8, activeLow: true, direction: "in"});
pinA.write(false);
bValue = pinB.read();
assert(bValue, "activeLow: true");
