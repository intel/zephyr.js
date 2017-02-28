// Copyright (c) 2017, Intel Corporation.

var performance = require("performance");

console.log("\nTesting console.info...");
console.info("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.info(" Booleans:", true, false);
console.info("Functions:", function () {});
console.info("  Numbers:", 1, -1, 123.456, -123.456);
console.info("  Objects:", {}, {foo: 'bar'});
console.info("  Strings:", '', "", 'abc', "def");  // expect: "  abc def"
console.info("     Misc:", null, undefined);

console.log("\nTesting console.warn...");
console.warn("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.warn(" Booleans:", true, false);
console.warn("Functions:", function () {});
console.warn("  Numbers:", 1, -1, 123.456, -123.456);
console.warn("  Objects:", {}, {foo: 'bar'});
console.warn("  Strings:", '', "", 'abc', "def");  // expect: "  abc def"
console.warn("     Misc:", null, undefined);

console.log("\nTesting console.assert... ");
var consoleStr = "abcd";
var longstr =
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "1234567890123456";  // length 256

console.log("assert(true): expected result: nothing");
console.assert(true);

console.log("\nassert(false, 'false assertion'): " +
            "expected result: AssertionError with false assertion");
try {
    console.assert(false, 'false assertion');
} catch (e) {
    console.log("Caught error: " + e.name + ": " + e.message);
}

console.log("\nassert(string): " +
            "expected result: TypeError with invalid parameter");
try {
    console.assert(consoleStr);
} catch (e) {
    console.log("Caught error: " + e.name + ": " + e.message);
}

console.log("\nassert(false, longstr): " +
            "expected result: AssertionError with long string");
try {
    console.assert(false, longstr);
} catch (e) {
    console.log("Caught error: " + e.name + ": " + e.message);
}

console.log("\nassert(false, 1024): " +
            "expected result: AssertionError with number value 1024");
try {
    console.assert(false, 1024);
} catch (e) {
    console.log("Caught error: " + e.name + ": " + e.message);
}

console.log("\nTesting console.time and console.timeEnd... ");
var before = performance.now();

console.log("timer: 20 timers run at the same time\n");
for (var i = 1; i < 21; i++) {
    console.time("Timer" + i);
}

setTimeout(function() {
    for (var j = 1; j < 20; j++) {
        console.timeEnd("Timer" + j);

        var after = performance.now();
        var diff = (after - before) | 0;
        console.log("expected result: " + (diff - 20) +
                    "ms ~ " + (diff + 20) + "ms\n");
    }

    console.log("timer: reposition the starting time");
    console.time("Timer20");
    console.timeEnd("Timer20");
    console.log("expected result: 0ms\n");

    console.log("timer: set invalid timer name");
    var Timer21;

    try {
        console.time(Timer21);
        console.timeEnd(Timer21);
    } catch (e) {
        console.log("expected result: TypeError with invalid argument");
        console.log("Caught error: " + e.name + ": " + e.message + "\n");
    }

    console.log("timer: set timer name as empty string");
    var Timer22 = "";

    console.time(Timer22);
    setTimeout(function() {
        console.timeEnd(Timer22);
        console.log("expected result: 1020ms\n");

        console.log("Testing completed");
    }, 1000);
}, 3000);
