// Copyright (c) 2016, Intel Corporation.

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
