// Copyright (c) 2016, Intel Corporation.

// AIO tests
// Sample simulates an analog voltage output by control GPIO pin:IO2.

// Hardware Requirements:
//   - Arduino 101
// Wiring:
//   - Wire IO2 to A0

console.log("Testing AIO APIs...");
console.log("NOTE: Make sure IO2 is connected to A0!");

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

// import aio and gpio module
var aio = require("aio");
var gpio = require("gpio");
var pins = require("arduino101_pins");


// Interface: AIOPin open(AIOInit init);
var test_open_true = "Use valid argument in function open(AIOInit).";
var test_open_false = "Use invalid argument in function open(AIOInit).";
// use valid argument
try {
    var A0 = aio.open({ pin: pins.A0 });
    assert(true, test_open_true);
} catch(e) {
    assert(false, test_open_true);
}
// use invalid argument
try {
    var A1 = aio.open({ pin: A1 });
    assert(false, test_open_false);
} catch(e) {
    assert(true, test_open_false + " and the error logs are as above");
}

// Function: unsigned long read()
var toggle = false;
var i = 0, times = 10, count = 0;
var IO2 = gpio.open({ pin: pins.IO2, direction: 'out' });

for (; i < times; i++) {
    toggle = !toggle;
    IO2.write(toggle);
    rawValue = A0.read();

    if (toggle) {
        if (rawValue >= 4000) {
            count ++;
        }
    } else {
        if (rawValue <= 100) {
            count ++;
        }
    }
}

assert(count == times, "read() function");

count = 0;
i = 0;

// Function: void on(string eventType, ReadCallback callback);
// Registe ReadCallback
var onCount = 0, oldOnCount = 0;
A0.on('change', function() {
    onCount ++;
});

// Function: unsigned long readAsync()
var test_readAsync = setInterval(function () {
    i ++;
    toggle = !toggle;
    IO2.write(toggle);

    A0.readAsync(function(rawValue) {
        console.log("...");
        if (toggle) {
            if (rawValue >= 4000) {
                count ++;
            }
        } else {
            if (rawValue <= 100) {
                count ++;
            }
        }

        // Discard ReadCallback
        if (i == 6) {
            oldOnCount = onCount;
            if (onCount == 5) {
                assert(true, "Function void on(string eventType, ReadCallback callback) works fine");
            } else if (onCount < 5) {
                assert(false, "Read too fast and the ReadCallback failed to be called.");
            } else {
                assert(false, "Gets called periodically even when AIO hasn't changed.");
            }
            A0.on('change', null);
        }

        if (i == times) {
            clearInterval(test_readAsync);
            assert(count == times, "readAsync() function");
            count = 0;
        }
    });

}, 1000);

setTimeout(function() {
    assert(onCount == oldOnCount, "on(string eventType, null) function");
    onCount = 0;

    IO2.write(true);
    A0.read();

    A0.on('change', function() {
        onCount ++;
    });

    // Function: void close()
    A0.close();
}, 12000);

setTimeout(function() {
    IO2.write(false);
    A0.read();

    assert(onCount == 0, "close() function");

    console.log("TOTAL: " + passed + " of " + total + " passed");

}, 15000);
