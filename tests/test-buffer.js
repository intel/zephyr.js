// Copyright (c) 2016-2017, Intel Corporation.

// Buffer Testing

var assert = require("Assert.js");

// Attribute: readonly unsigned long length
var buff;
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

assert.throws(function () {
    // unsupported encoding
    buff.toString("utf8");
}, "Error thrown when 'hex' is not given to toString()");

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
