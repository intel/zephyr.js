// Copyright (c) 2016, Intel Corporation.

// Stress test for buffer code
console.log("Buffer stress test...");

// This was useful for catching memory leaks in the original Buffer code
for (var i = 1; i <= 1000; i++) {
    console.log("Allocating buffer", i);

    var mybuf = new Buffer(8);
    for (var j = 0; j < 8; j++)
        mybuf.writeUInt8(j * 4, j);
}
