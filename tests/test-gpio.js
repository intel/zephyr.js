// Copyright (c) 2016, Intel Corporation.

// Testing GPIO APIs

// Requirements:
//   - Wire IO2 to IO4

var total = 0;
var passed = 0;

function assert(actual, description) {
    total += 1;

    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    print(label + " - " + description);
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

var gpio = require("gpio");
var pins = require("arduino101_pins");

// test GPIO open
var pinA = gpio.open({ pin: pins.IO2 });
var pinB = gpio.open({ pin: pins.IO4, direction: "in", edge: "any" });
assert(pinA != null && typeof pinA == "object",
       "open: defined pin and default as 'out' direction");

assert(pinB != null && typeof pinB == "object", "open: set edge with 'any'");

// test GPIOPin read and write
var outputValue = false;
pinA.write(outputValue);
var inputValue = pinB.read();
assert(inputValue === outputValue, "gpiopin: write and read");

inputValue = pinA.read();
assert(!inputValue, "read: output false");

pinB.write(!outputValue);
inputValue = pinB.read();
assert(!inputValue, "write: input true");

// test GPIOPin onchange
var times = 0;
pinA.write(false);

pinB.onchange = function (event) {
    times = times + 1;
};

pinA.write(true);
pinA.write(false);
assert(times == 2, "onchange: with 'any' edge");

// set onchange event as rising
pinB = gpio.open({ pin: pins.IO4, direction: "in", edge: "rising" });
times = 0;
pinA.write(true);
pinA.write(false);
pinA.write(true);
assert(times == 2, "onchange: with 'rising' edge");

// set onchange event as falling
pinB = gpio.open({ pin: pins.IO4, direction: "in", edge: "falling" });
times = 0;
pinA.write(false);
pinA.write(true);
pinA.write(false);
assert(times == 2, "onchange: with 'falling' edge");

// test GPIOPin close
times = 0;
pinB.close();
pinA.write(true);
pinA.write(false);
pinA.write(true);
assert(times == 0, "close: closed in onchange pin");

assert(pinB == null, "close: closed in GPIO pin");

expectThrow("write: invalid argument", function () {
    pinA.write(1);
});

expectThrow("open: undefined pin", function () {
    gpio.open({ pin: 123 });
});

// test GPIO openAsync
var pinX = null;
var pinY = null;
times = 0;

gpio.openAsync({ pin: pins.IO4, direction: "in", edge: "any" }).then(function(pin) {
    pinY = pin;
    assert(pinY != null && typeof pinY == "object",
           "openAsync: set edge with 'any'");

    pinY.onchang = function (event) {
        times = times + 1;
    };
});

gpio.openAsync({ pin: pins.IO2 }).then(function(pin) {
    pinX = pin;
    assert(pinX != null && typeof pinX == "object",
       "openAsync: defined pin and default as 'out' direction");

    pinX.write(true);
    assert(pinY.read() == true, "gpiopin: read and write in Async pin");

    pinX.write(false);
    pinX.write(true);
    assert(times == 3, "onchange: change event in Async pin");

    pinY.close();
    pinX.write(false);
    assert(times == 3, "close: closed in Async pin");
});

setTimeout(function () {
    print("TOTAL: " + passed + " of " + total + " passed");
}, 500);