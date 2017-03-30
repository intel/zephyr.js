// Copyright (c) 2016-2017, Intel Corporation.

// Buffer Testing

var assert = require("Assert.js");

// Attribute: readonly unsigned long length
var buff;
var lens = [[-1, 0],
            [0, 0],
            [8, 8],
            [8.5, 8],
            [4294967295, 4294967295],
            [4294967296, 4294967295]];
for(var i = 0; i < lens.length; i++) {
    try {
        buff = new Buffer(lens[i][0]);
        assert(buff.length === lens[i][1],
               "The length of Buffer(" + lens[i][0] + ") expected:" +
               lens[i][1] + " got:" + buff.length);
    } catch (error) {
        console.log("Buffer: too long or not enough available memory");
        assert(typeof buff === "undefined",
               "The length of Buffer(" + lens[i][0] +
               ") expected: 'undefined'" + " got:" + typeof buff);
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
for(var i = 0; i < uints.length; i++) {
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
}, "Error thrown when the offset of writeUInt8() " +
   "is outside the bounds of the Buffer");

assert.throws(function () {
    // read beyond end of buffer
    buff.readUInt8(uints.length);
}, "Error thrown when the offset of readUInt8() " +
   "is outside the bounds of the Buffer");

// Function: string toString(string encoding)
var hexs = [[0, "00"],
            [17, "11"],
            [255, "ff"],
            [256, "00"],
            [511, "ff"]];
var expected = "";
buff = new Buffer(hexs.length);
for(var i = 0; i < hexs.length; i++) {
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

var buff = new Buffer(4);
var writeValue = -0.232;
// write negative float value (14 bit precision) as uint32 by disabling noAssert.
buff.writeUInt32BE((writeValue * Math.pow(2, 32-14)) << 1, 0, true);
var readValue = (buff.readUInt32BE(0) >> 1) / Math.pow(2, 32-14);
assert(writeValue.toPrecision(3) === readValue.toPrecision(3),
       "The value of readUInt32BE() with precision 3 expected: "
       + writeValue + " got: " + readValue.toPrecision(3));

assert.result();
