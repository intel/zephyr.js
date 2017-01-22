// Copyright (c) 2016-2017, Intel Corporation.

var consoleNum = 1024;
var consoleStr = "abcd";
console.log("Testing console.log...");
console.log("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.log(" Booleans:", true, false);
console.log("Functions:", function () {});
console.log("  Numbers:", 1, -1, 123.456, -123.456);
console.log("  Objects:", {}, {foo: 'bar'});
console.log("  Strings:", '', "", 'abc', "def");
console.log("     Misc:", null, undefined);

console.log("\nTesting console.error...");
console.error("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.error(" Booleans:", true, false);
console.error("Functions:", function () {});
console.error("  Numbers:", 1, -1, 123.456, -123.456);
console.error("  Objects:", {}, {foo: 'bar'});
console.error("  Strings:", '', "", 'abc', "def");
console.error("     Misc:", null, undefined);

console.log("\nTesting console.info...");
console.info("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.info(" Booleans:", true, false);
console.info("Functions:", function () {});
console.info("  Numbers:", 1, -1, 123.456, -123.456);
console.info("  Objects:", {}, {foo: 'bar'});
console.info("  Strings:", '', "", 'abc', "def");
console.info("     Misc:", null, undefined);

console.log("\nTesting console.warn...");
console.warn("   Arrays:", [], [1, 2, 3], ['a', 0, [1, 2, 3]]);
console.warn(" Booleans:", true, false);
console.warn("Functions:", function () {});
console.warn("  Numbers:", 1, -1, 123.456, -123.456);
console.warn("  Objects:", {}, {foo: 'bar'});
console.warn("  Strings:", '', "", 'abc', "def");
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
console.assert(true);
console.log("assert(true): expected result nothing");

try {
    console.assert(false, "My lover!");
} catch (e) {
    console.log("assert(false, 'My lover!'): expected result throw error");
    console.log("ERROR: " + e.name + " - " + e.message);
}

console.assert(consoleStr);
console.log("assert(string): expected result [ERROR] invalid parameters");

console.assert(false, longstr);
console.log("assert(false, longstr): " +
            "expected result [ERROR] string is too long");

console.assert(false, consoleNum);
console.log("assert(false, Num): " +
            "expected result [ERROR] message parameter must be a string");

console.log("\nTesting constructor to create...");
var stdout, stderr;
var myConsole = new Console(stdout, stderr);
myConsole.log("create new console as 'myConsole'");
