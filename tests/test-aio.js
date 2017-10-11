// Copyright (c) 2016-2017, Intel Corporation.

// Testing AIO APIs

console.log("Wire IO4 to A0!");

// Pre-conditions
var aio = require("aio");
var gpio = require("gpio");
var pins = require("arduino101_pins");
var assert = require("Assert.js");

// test AIO open
assert.throws(function() {
    aio.open({pin: 1024});
}, "open: undefined pin");

var pinA = aio.open({pin: pins.A0});
assert(pinA !== null && typeof pinA === "object", "open: defined pin");

var readFlag = 0;
var readAsyncflag = 0;
var waveFlag = 0;
var OldwaveFlag = 1;
var readTimes = 0;
var readCount = 0;

var pinB = gpio.open({pin: "IO4", mode: 'out'});
pinB.write(1);

// test AIOPin on
pinA.on("change", function (value) {
    if (value <= 100) {
        waveFlag = 0;
    }

    if (value >= 4000) {
        waveFlag = 1;
    }

    if (waveFlag !== OldwaveFlag) {
        console.log("aio: '" + waveFlag + "' change to '" + OldwaveFlag + "'");

        OldwaveFlag = waveFlag;
        readCount++;
    }
});

var valueChanged = function(value) {
    readCount++;
}

// test AIOPin read
var onInterval = setInterval(function () {
    pinB.write(readFlag);
    readFlag = 1 - readFlag;
    readTimes++;

    if (readTimes === 4) {
        assert(readCount === readTimes - 1, "aiopin: event type as 'change'");
    }

    if (readTimes === 5) {
        assert(readCount === 4, "aiopin: read value");
        pinA.close();
    }

    // test AIOPin close
    if (readTimes === 6) {
        assert(readCount === 4, "aiopin: be closed");

        // test AIOPin callback as 'null'
        readCount = 0;

        pinA.on("change", valueChanged);

        pinA.removeListener("change", valueChanged);
    }

    if (readTimes === 8) {
        assert(readCount === 0,
               "aiopin: removed listener and be discarded");
    }

    // test AIOPin readAsync
    if (readTimes === 10) {
        pinA.readAsync(function (rawValue) {
            if (rawValue >= 4000) {
                readAsyncflag = 1;
            }

            if (rawValue <= 100) {
                readAsyncflag = 0;
            }

            assert(readAsyncflag !== readFlag,
                   "aiopin: read by asynchronously");

            assert.result();
        });

        clearInterval(onInterval);
    }
}, 5000);
