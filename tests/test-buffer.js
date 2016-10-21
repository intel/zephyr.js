// Copyright (c) 2016, Intel Corporation.

// Buffer Testing

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

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (err) {
        threw = true;
    }
    assert(threw, description);
}

// Attribute: readonly unsigned long length
var buff;
var lens = [[-1, 0],
            [0, 0],
            [8, 8],
            [8.5, 8],
            [4294967295, 4294967295],
            [4294967296, 4294967295]];
for(var i = 0; i < lens.length; i++) {
    buff = new Buffer(lens[i][0]);
    assert(buff.length === lens[i][1],
           "The length of Buffer(" + lens[i][0] + ") expected:" + lens[i][1] +
           " got:" + buff.length);
}

buff = new Buffer(8);
var original = buff.length;
buff.length = buff.length + 1;
assert(buff.length === original, "The length of Buffer is readonly");


// Function: writeUInt8(unsigned char value, unsigned long offset)
//           unsigned char readUInt8(unsigned long offset)
var uints = [[-1, 0],
             [0, 0],
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


expectThrow("Error thrown when the offset of writeUInt8() " +
            "is outside the bounds of the Buffer", function () {
    // write beyond end of buffer
    buff.writeUInt8(0, uints.length);
});

expectThrow("Error thrown when the offset of readUInt8() " +
            "is outside the bounds of the Buffer", function () {
    // read beyond end of buffer
    buff.readUInt8(uints.length);
});

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

expectThrow("Error thrown when 'hex' is not given to toString()", function () {
    // unsupported encoding
    buff.toString("utf8");
});

console.log("TOTAL: " + passed + " of " + total + " passed");
