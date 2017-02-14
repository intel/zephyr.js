// Copyright (c) 2016-2017, Intel Corporation.

var consoleNum = 1024;
var consoleStr = "abcd";
console.log("Testing console.log...");
console.log("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.log(" Booleans:", true, false);
console.log("Functions:", function () {});
console.log("  Numbers:", 1, -1, 123.456, -123.456);
console.log("  Objects:", {}, {foo: 'bar'});
console.log("  Strings:", '', "", 'abc', "def");  // expect: "  abc def"
console.log("     Misc:", null, undefined);

console.log("\nTesting console.error...");
console.error("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.error(" Booleans:", true, false);
console.error("Functions:", function () {});
console.error("  Numbers:", 1, -1, 123.456, -123.456);
console.error("  Objects:", {}, {foo: 'bar'});
console.error("  Strings:", '', "", 'abc', "def");  // expect: "  abc def"
console.error("     Misc:", null, undefined);

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

console.log("\nTesting a long string...");
var longstr =
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345678901234567890123456789012345678901234567890" +
    "123456789012345";  // length 255
console.log(longstr);

console.log("\nTesting a string too long...");
longstr += '6';  // length 256
console.log(longstr);

console.log("\nTesting console.assert... ");
console.log("assert(true): expected result: nothing");
console.assert(true);

console.log("\nassert(false, 'false assertion'): " +
            "expected result: AssertionError");
try {
    console.assert(false, 'false assertion');
} catch (e) {
    console.log("Caught error: " + e.name + ": " + e.message);
}

console.log("\nassert(string): expected result: throw TypeError");
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
