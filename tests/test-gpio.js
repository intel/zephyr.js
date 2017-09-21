// Copyright (c) 2016-2017, Intel Corporation.

// Testing GPIO APIs

var gpio = require("gpio");
var assert = require("Assert.js");

if (gpio.mock) {
    gpio.wire(gpio.open(7), gpio.open(10));
}
else {
    // NOTE: changed from pin 8 to 10 so this test can work on k64f too
    console.log("Hardware setup: Wire pin 7 to pin 10");
}

var pinA, pinB, aValue, bValue;

// test GPIO open
pinA = gpio.open(7);
pinB = gpio.open({pin: 10, mode: "in", edge: "none"});

assert(pinA != null && typeof pinA == "object",
      "open: defined pin and default as 'out' direction");

assert(pinB != null && typeof pinB == "object",
      "open: defined pin with direction 'in'");

assert.throws(function () {
    gpio.open(1024);
}, "open: invalid pin");

// test GPIOPin read and write
pinA.write(1);
bValue = pinB.read();
assert(bValue, "gpiopin: write and read");

// reading an output pin may not be valid; this is probably a bad test
aValue = pinA.read();
assert(aValue, "gpiopin: read output pin");

pinB.write(0);
bValue = pinB.read();
assert(bValue, "gpiopin: write input pin");

assert.throws(function () {
    // booleans allowed temporarily as deprecation path, so try another value
    pinA.write("1");
}, "gpiopin: write invalid argument");

// test activeLow
pinB = gpio.open({pin: 10, mode: "in", activeLow: true});
pinA.write(0);
bValue = pinB.read();
assert(bValue, "activeLow: true");

// test GPIOPin onchange
var changes = [
    ["falling", 1, 1],
    ["rising", 1, 0],
    ["any", 1, 0]
];
var mark = 0;

var edgeInterval = setInterval(function () {
    var count = 0;
    pinA = gpio.open(7);
    pinA.write(changes[mark][2]);
    pinB = gpio.open({pin: 10, mode: "in", edge: changes[mark][0]});

    pinB.onchange = function () {
        count++;
        pinB.close();
    };

    pinA.write(1 - changes[mark][2]);

    setTimeout(function () {
        assert(count == changes[mark][1],
               "gpiopin: onchange with edge '" + changes[mark][0] + "'");

        if (changes[mark][0] == "any") {
            // test GPIOPin close
            pinA.write(1 - changes[mark][2]);
            assert(count == changes[mark][1], "gpiopin: close onchange");
        }

        mark = mark + 1;

        if (mark == 3) {
            assert.result();
            clearInterval(edgeInterval);
        }
    }, 100);
}, 200);
