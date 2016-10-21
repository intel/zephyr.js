// Copyright (c) 2016, Intel Corporation.

// Test code for Buffer
console.log("Buffer test...");

var buf = new Buffer(8);
console.log("Buffer object:", buf);

buf.writeUInt8(222);  // de
buf.writeUInt8(173, 1);  // ad
buf.writeUInt8(190, 2);  // be
buf.writeUInt8(239, 3);  // ef

// should print deadbeef followed by four random uninitialized bytes
console.log("Buffer contents:", buf.toString('hex'));
