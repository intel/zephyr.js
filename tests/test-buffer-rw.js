// Copyright (c) 2016, Intel Corporation.

// Buffer read/write tests

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

// setup buffer with 0xdeadbeef, then 0xfadedcab
var buf = Buffer(8);
buf.writeUInt8(0xde, 0);
buf.writeUInt8(0xad, 1);
buf.writeUInt8(0xbe, 2);
buf.writeUInt8(0xef, 3);
buf.writeUInt8(0xfa, 4);
buf.writeUInt8(0xde, 5);
buf.writeUInt8(0xdc, 6);
buf.writeUInt8(0xab, 7);

// test readUInt8 / writeUInt8
expectThrow("readUInt8: invalid argument", function () {
    buf.readUInt8('whale');
});
expectThrow("readUInt8: out of bounds", function () {
    buf.readUInt8(9);
});

expectThrow("writeUInt8: invalid first argument", function () {
    buf.writeUInt8('bowl');
});
expectThrow("writeUInt8: invalid second argument", function () {
    buf.writeUInt8(42, 'petunias');
});
expectThrow("readUInt8: out of bounds", function () {
    buf.writeUInt8(42, 9);
});

assert(buf.readUInt8()  == 0xde, "readUInt8: read a byte, no arg");
assert(buf.readUInt8(0) == 0xde, "readUInt8: read a byte, offset 0");
assert(buf.readUInt8(4) == 0xfa, "readUInt8: read a byte, offset 4");

// test readUInt16BE
assert(buf.readUInt16BE()  == 0xdead, "readUInt16BE: read short, no arg");
assert(buf.readUInt16BE(0) == 0xdead, "readUInt16BE: read short, offset 0");
assert(buf.readUInt16BE(4) == 0xfade, "readUInt16BE: read short, offset 4");

// test readUInt16LE
assert(buf.readUInt16LE()  == 0xadde, "readUInt16LE: read short, no arg");
assert(buf.readUInt16LE(0) == 0xadde, "readUInt16LE: read short, offset 0");
assert(buf.readUInt16LE(4) == 0xdefa, "readUInt16LE: read short, offset 4");

// test readUInt32BE
assert(buf.readUInt32BE()  == 0xdeadbeef, "readUInt32BE: read long, no arg");
assert(buf.readUInt32BE(0) == 0xdeadbeef, "readUInt32BE: read long, offset 0");
assert(buf.readUInt32BE(4) == 0xfadedcab, "readUInt32BE: read long, offset 4");

// test readUInt32LE
assert(buf.readUInt32LE()  == 0xefbeadde, "readUInt32LE: read long, no arg");
assert(buf.readUInt32LE(0) == 0xefbeadde, "readUInt32LE: read long, offset 0");
assert(buf.readUInt32LE(4) == 0xabdcdefa, "readUInt32LE: read long, offset 4");

// test writeUInt16BE
buf.writeUInt16BE(0xbead)
assert(buf.readUInt8(0) == 0xbe && buf.readUInt8(1) == 0xad,
       "writeUInt16BE: write short, no offset");
buf.writeUInt16BE(0xdeaf, 0)
assert(buf.readUInt8(0) == 0xde && buf.readUInt8(1) == 0xaf,
       "writeUInt16BE: write short, offset 0");
buf.writeUInt16BE(0xfeed, 4)
assert(buf.readUInt8(4) == 0xfe && buf.readUInt8(5) == 0xed,
       "writeUInt16BE: write short, offset 4");

// test writeUInt16LE
buf.writeUInt16LE(0xbead)
assert(buf.readUInt8(0) == 0xad && buf.readUInt8(1) == 0xbe,
       "writeUInt16LE: write short, no offset");
buf.writeUInt16LE(0xdeaf, 0)
assert(buf.readUInt8(0) == 0xaf && buf.readUInt8(1) == 0xde,
       "writeUInt16LE: write short, offset 0");
buf.writeUInt16LE(0xfeed, 4)
assert(buf.readUInt8(4) == 0xed && buf.readUInt8(5) == 0xfe,
       "writeUInt16LE: write short, offset 4");

// test writeUInt32BE
buf.writeUInt32BE(0xdeadbeef)
assert(buf.readUInt8(0) == 0xde && buf.readUInt8(1) == 0xad &&
       buf.readUInt8(2) == 0xbe && buf.readUInt8(3) == 0xef,
       "writeUInt32BE: write long, no offset");
buf.writeUInt32BE(0xfadedcab, 0)
assert(buf.readUInt8(0) == 0xfa && buf.readUInt8(1) == 0xde &&
       buf.readUInt8(2) == 0xdc && buf.readUInt8(3) == 0xab,
       "writeUInt32BE: write long, offset 0");
buf.writeUInt32BE(0xbeadface, 4)
assert(buf.readUInt8(4) == 0xbe && buf.readUInt8(5) == 0xad &&
       buf.readUInt8(6) == 0xfa && buf.readUInt8(7) == 0xce,
       "writeUInt32BE: write long, offset 4");

// test writeUInt32LE
buf.writeUInt32LE(0xdeadbeef)
assert(buf.readUInt8(0) == 0xef && buf.readUInt8(1) == 0xbe &&
       buf.readUInt8(2) == 0xad && buf.readUInt8(3) == 0xde,
       "writeUInt32LE: write long, no offset");
buf.writeUInt32LE(0xfadedcab, 0)
assert(buf.readUInt8(0) == 0xab && buf.readUInt8(1) == 0xdc &&
       buf.readUInt8(2) == 0xde && buf.readUInt8(3) == 0xfa,
       "writeUInt32LE: write long, offset 0");
buf.writeUInt32LE(0xbeadface, 4)
assert(buf.readUInt8(4) == 0xce && buf.readUInt8(5) == 0xfa &&
       buf.readUInt8(6) == 0xad && buf.readUInt8(7) == 0xbe,
       "writeUInt32LE: write long, offset 4");

console.log("TOTAL: " + passed + " of " + total + " passed");
