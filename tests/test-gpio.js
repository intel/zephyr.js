// Copyright (c) 2016-2017, Intel Corporation.

// Testing GPIO APIs

var gpio = require("gpio");
var assert = require("Assert.js");
var board = require("board");

// test GPIO open
if (gpio.mock) {
    // support Linux
    gpio.wire(gpio.open(7), gpio.open(10));
} else {
    console.log("Wire pin 7 to pin 10");
}

if (board.name === "arduino_101" ||
    board.name === "linux (partially simulating arduino_101)") {
    var pinNames = [
        [4, "number"],
        ["4", "string"],
        ["GPIO_0.4", "string"],
        ["IO4", "string"],
        ["LED0", "string"]
    ];
    for (var i = 0; i < pinNames.length; i++) {
        var pinOpen = gpio.open(pinNames[i][0]);

        assert(pinOpen !== null && typeof pinOpen === "object",
              "open: defined pin with " + pinNames[i][1] +
               " '" + pinNames[i][0] + "'");
    }
} else {
    var pinNames = [
        [4, "number"],
        ["4", "string"],
        ["GPIO_0.4", "string"],
        ["D4", "string"],
        ["LEDR", "string"],
        ["LEDG", "string"],
        ["LEDB", "string"],
        ["SW2", "string"],
    ];
    for (var i = 0; i < pinNames.length; i++) {
        var pinOpen = gpio.open(pinNames[i][0]);

        assert(pinOpen !== null && typeof pinOpen === "object",
              "open: defined pin with " + pinNames[i][1] +
               " '" + pinNames[i][0] + "'");
    }
}

var pinA, pinB, writeValue, aValue, bValue, WRFlag;

pinA = gpio.open(7);
assert(pinA != null && typeof pinA == "object",
      "open: default value - mode: 'out' activeLow: 'true'");

pinB = gpio.open({pin: 10, mode: "in", edge: "none", activeLow: false});
assert(pinB != null && typeof pinB == "object",
      "open: set value - mode: 'in' edge: 'none' activeLow: 'false'");

assert.throws(function () {
    gpio.open(1024);
}, "open: invalid pin");

// test activeLow
pinA.write(1);
bValue = pinB.read();
assert(bValue === 1, "activeLow: false");

// test GPIOPin read and write
writeValue = 1;
WRFlag = true;

for (var i = 0; i < 10; i++) {
    writeValue = 1 - writeValue;

    pinA.write(writeValue);
    bValue = pinB.read();

    if (writeValue !== bValue) {
        WRFlag = false;
    }
}
assert(WRFlag, "gpiopin: write and read 10 times");

pinA.write(1);
pinB.write(0);
bValue = pinB.read();
assert(bValue, "gpiopin: write input pin");

assert.throws(function () {
    pinA.write(pinA);
}, "gpiopin: write invalid argument");

// test GPIOPin close
pinA.write(1);
pinB.close();
assert.throws(function () {
    bValue = pinB.read();
}, "close: not be used for reading");

pinA.close();
assert.throws(function () {
    pinA.write(1);
}, "close: not be used for writing");

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
