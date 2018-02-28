// Copyright (c) 2016-2018, Intel Corporation.

// Buffer Testing
console.log("Test buffer APIs");

var assert = require("Assert.js");

var buff;

// Create buffer: array (not test out of memory)
var lens = [1, 10, 100, 1024];
buff = new Buffer(lens);
assert(buff !== null && typeof buff === "object",
       "Creating buffer with array [1, 10, 100, 1024]");
assert(buff.length === 4,
       "Creating buffer of length " + buff.length + " with array");

var lens = [[1, 10], "ZJS", "w"];
buff = new Buffer(lens);
assert(buff !== null && typeof buff === "object" && buff.length === 3,
       "Creating buffer with non-numeric array and " +
       "the array will be treated as 0");

// Create buffer: string (not test out of memory)
var lens = ["w", "ZJS", "1234567890", "!@#$%^&*()"];
for (var i = 0; i < lens.length; i++) {
    buff = new Buffer(lens[i]);
    assert(buff !== null && typeof buff === "object",
           "Creating buffer with string " + lens[i]);
}

// Attribute: readonly unsigned long length
var lens = [[-1, 0],
            [0, 0],
            [8, 8],
            [8.49, 8],  // now it does rounding
            [8.5, 9]];
for (var i = 0; i < lens.length; i++) {
    buff = new Buffer(lens[i][0]);
    assert(buff.length === lens[i][1],
           "Creating buffer of length " + lens[i][0] + " expected:" +
           lens[i][1] + " got:" + buff.length);
}

// determine whether it's 32- or 64-bit platform
var bits32 = false;
try {
    buff = new Buffer(0x7fffffff);
    assert(true, "64-bit system (2^31-1 length buffer allowed)");
}
catch (e) {
    if (e instanceof RangeError) {
        assert(true, "32-bit system (2^31-1 length buffer out of range)");
        bits32 = true;
    }
    else {
        assert(false, "while testing range: " + e);
    }
}

// thrown errors are allowed on these lengths; boolean is whether it should be
//   a RangeError or not
lens = [[0x03fffffff, false],
        [0x040000000, bits32],
        [0x07fffffff, bits32],
        [0x080000000, true],
        [0x0ffffffff, true],
        [0x100000000, true]];

for (var i = 0; i < lens.length; i++) {
    try {
        buff = new Buffer(lens[i][0]);
        if (lens[i][1]) {
            assert(false, "Creating buffer of length " + lens[i][0] +
                   " succeeded with length "  + buff.length +
                   ", but should fail with RangeError");
        }
        else {
            assert(buff.length === lens[i][0],
                   "Creating buffer of length " + lens[i][0] + " succeeded");
        }
    }
    catch (e) {
        if (lens[i][1] && !(e instanceof RangeError)) {
            assert(false, "Creating buffer of length " + lens[i][0] +
                   " failed with: " + e +
                  ", but should fail with RangeError");
        }
        else {
            // allow errors
            assert(true, "Creating buffer of length " + lens[i][0] +
                   " failed with: " + e);
        }
    }
}

buff = new Buffer(8);
var original = buff.length;
buff.length = buff.length + 1;
assert(buff.length === original, "The length of Buffer is readonly");


// Function: writeUInt8(unsigned char value, unsigned long offset)
//           unsigned char readUInt8(unsigned long offset)
var uints = [[0, 0],
             [1, 1],
             [1.6, 1],
             [255, 255],
             [256, 0],
             [300, 44]];
buff = new Buffer(uints.length);
for (var i = 0; i < uints.length; i++) {
    var actual;
    if (i == 0) {
        // not given offset
        buff.writeUInt8(uints[i][0]);
        actual = buff.readUInt8();
    } else {
        buff.writeUInt8(uints[i][0], i);
        actual = buff.readUInt8(i);
    }
    assert(actual === uints[i][1],
           "The value of readUInt8() expected:" + uints[i][1] +
           " got:" + actual + " when writeUInt8(" + uints[i][0] + ")");
}


assert.throws(function () {
    // write beyond end of buffer
    buff.writeUInt8(0, uints.length);
}, "Error thrown when offset of writeUInt8() is outside bounds of Buffer");

assert.throws(function () {
    // read beyond end of buffer
    buff.readUInt8(uints.length);
}, "Error thrown when offset of readUInt8() is outside bounds of Buffer");

// Function: string toString(string encoding)
var hexs = [[0, "00"],
            [17, "11"],
            [255, "ff"],
            [256, "00"],
            [511, "ff"]];
var expected = "";
buff = new Buffer(hexs.length);
for (var i = 0; i < hexs.length; i++) {
    buff.writeUInt8(hexs[i][0], i);
    expected += hexs[i][1];
}
assert(buff.toString('hex') === expected,
       "The value of toString('hex') expected:" + expected +
       " got:" + buff.toString('hex'));

// Test fill function w/ string source
buff = new Buffer(10);
buff.fill('abc');
assert(buff.toString('hex') === '61626361626361626361', 'fill with string');

// Test fill function w/ integer source
buff.fill(257);
assert(buff.toString('hex') === '00000101000001010000', 'fill with int');

// Test fill function w/ buffer source
var srcbuf = new Buffer(3);
srcbuf.writeUInt8(1, 0);
srcbuf.writeUInt8(2, 1);
srcbuf.writeUInt8(3, 2);
buff.fill(srcbuf);
assert(buff.toString('hex') === '01020301020301020301', 'fill with buf');

// Test offset argument with some a chars at the end
buff.fill('a', 8);
assert(buff.toString('hex') === '01020301020301026161', 'fill offset end');

// Test end argument with some b chars in the middle
buff.fill('b', 5, 8);
assert(buff.toString('hex') === '01020301026262626161', 'fill offset middle');

// Test copy function
buff.fill(0);
srcbuf.writeUInt8(1, 0);
srcbuf.writeUInt8(2, 1);
srcbuf.writeUInt8(3, 2);

srcbuf.copy(buff);
assert(buff.toString('hex') === '01020300000000000000', 'copy full buf');

srcbuf.copy(buff, 3);
assert(buff.toString('hex') === '01020301020300000000', 'copy at offset');

srcbuf.copy(buff, 8, 1);
assert(buff.toString('hex') === '01020301020300000203', 'copy to end');

srcbuf.copy(buff, 6, 1, 2);
assert(buff.toString('hex') === '01020301020302000203', 'copy part');

assert.throws(function () {
    srcbuf.copy(buff, 2, 4);
}, "Error thrown copying beyond source buffer");

assert.throws(function () {
    buff.copy(srcbuf, 4, 6);
}, "Error thrown copying beyond target buffer");

assert.throws(function () {
    srcbuf.copy(buff, 9);
}, "Error thrown when copying would finish beyond target buffer");

assert.throws(function () {
    // unsupported encoding
    buff.toString('unicorn64');
}, "Error thrown with unsupported encoding in toString()");

var ubuf = new Buffer(20);
ubuf.writeUInt8(0xc2, 0);   // ¡
ubuf.writeUInt8(0xa1, 1);
ubuf.writeUInt8(0xc6, 2);   // Ƶ
ubuf.writeUInt8(0xb5, 3);
ubuf.writeUInt8(0xc4, 4);   // Ĵ
ubuf.writeUInt8(0xb4, 5);
ubuf.writeUInt8(0xc6, 6);   // Ƨ
ubuf.writeUInt8(0xa7, 7);
ubuf.writeUInt8(0x20, 8);   // space
ubuf.writeUInt8(0xc6, 9);   // Ʀ
ubuf.writeUInt8(0xa6, 10);
ubuf.writeUInt8(0xc7, 11);  // Ǿ
ubuf.writeUInt8(0xbe, 12);
ubuf.writeUInt8(0xc2, 13);  // ©
ubuf.writeUInt8(0xa9, 14);
ubuf.writeUInt8(0xc4, 15);  // Ķ
ubuf.writeUInt8(0xb6, 16);
ubuf.writeUInt8(0xc6, 17);  // Ƨ
ubuf.writeUInt8(0xa7, 18);
ubuf.writeUInt8(0x21, 19);  // !

assert(ubuf.toString() === '¡ƵĴƧ ƦǾ©ĶƧ!', 'toString with utf8 default');
assert(ubuf.toString('utf8') === '¡ƵĴƧ ƦǾ©ĶƧ!', 'toString with utf8 encoding');
assert(ubuf.toString('ascii') == "B!F5D4F' F&G>B)D6F'!",
       'toString with ascii encoding');

/*
 * We don't support Math functions currently or noAssert option to buffer writes
 *
var buff = new Buffer(4);
var writeValue = -0.232;
// write negative float value (14 bit precision) as uint32 by disabling noAssert
buff.writeUInt32BE((writeValue * Math.pow(2, 32-14)) << 1, 0);
var readValue = (buff.readUInt32BE(0) >> 1) / Math.pow(2, 32-14);
assert(writeValue.toPrecision(3) === readValue.toPrecision(3),
       "The value of readUInt32BE() with precision 3 expected: "
       + writeValue + " got: " + readValue.toPrecision(3));
*/

assert.result();
