// Copyright (c) 2016, Intel Corporation.

// Testing GPIO APIs

// Pre-conditions
console.log("Wire IO2 to IO4");

var gpio = require("gpio");
var pins = require("arduino101_pins");

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
    pinA = gpio.open({ pin: pins.IO2 });
    pinA.write(changes[mark][2]);
    pinB = gpio.open({
        pin: pins.IO4,
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
            console.log("TOTAL: " + passed + " of " + total + " passed");
            clearInterval(edgeInterval);
        }
    }, 1000);
}, 2000);

// test GPIO open
pinA = gpio.open({ pin: pins.IO2 });
pinB = gpio.open({ pin: pins.IO4, direction: "in" });

assert(pinA != null && typeof pinA == "object",
      "open: defined pin and default as 'out' direction");

assert(pinB != null && typeof pinB == "object",
      "open: defined pin with direction 'in'");

expectThrow("open: invalid pin", function () {
    gpio.open({ pin: 1024 });
});

// test GPIOPin read and write
pinA.write(true);
bValue = pinB.read();
assert(bValue, "gpiopin: write and read");

aValue = pinA.read();
assert(aValue, "gpiopin: read output pin");

pinB.write(false);
bValue = pinB.read();
assert(bValue, "gpiopin: write input pin");

expectThrow("gpiopin: write invalid argument", function () {
    pinA.write(1);
});

// test activeLow
pinB = gpio.open({ pin: pins.IO4, activeLow:true, direction: "in" });
pinA.write(false);
bValue = pinB.read();
assert(bValue, "activeLow: true");

// test GPIO openAsync
gpio.openAsync({ pin: pins.IO2 }).then(function (pin2) {
    assert(pin2 != null && typeof pin2 == "object",
          "openAsync: defined pin and default as 'out' direction");
    gpio.openAsync({ pin: pins.IO4, direction: "in", edge: "any" })
        .then(function (pin4) {
            pin4.onchange = function (event) {
                assert(true, "gpiopin: onchange in openAsync");
                pin4.close();
            };
            pin2.write(true);
            var pin4v = pin4.read();
            assert(pin4v, "gpiopin: read and write in openAsync");
    });
    pin2.write(false);
});
