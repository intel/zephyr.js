// Copyright (c) 2017, Intel Corporation.

var fs = require('fs');
var assert = require("Assert.js");

// Clean up any test files left from previous tests
var stats = fs.statSync('testfile.txt');
if (stats && (stats.isFile() || stats.isDirectory())) {
    fs.unlinkSync('testfile.txt');
    console.log('removing testfile.txt');
}

// test file creation
var fd = fs.openSync('testfile.txt', 'a');
fs.closeSync(fd);
var fd_stats = fs.statSync('testfile.txt');
var success = false;
if (fd_stats && fd_stats.isFile()) {
	success = true;
}
assert(success, "create file");

// test file removal
fs.unlinkSync('testfile.txt');
fd_stats = fs.statSync('testfile.txt');
success = false;
if (!fd_stats || !fd_stats.isFile()) {
	success = true;
}
assert(success, "remove file");

//test invalid file open (on a non-existing file)
assert.throws(function () {
    fd = fs.openSync('testfile.txt', 'r');
}, "open(r) on non-existing file");

assert.throws(function () {
    fd = fs.openSync('testfile.txt', 'r+');
}, "open(r+) on non-existing file");

// Test appending options
fs.writeFileSync("testfile.txt", new Buffer("test"));

fd = fs.openSync('testfile.txt', 'a');

// test reading from 'a' file
assert.throws(function() {
    var rbuf = new Buffer(wbuf.length);
    var rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);
}, "can't read from append file");

// test writing to end of 'a' file
var wbuf = new Buffer('write');
var wlen = fs.writeSync(fd, wbuf, 0, wbuf.length, 0);
assert((wbuf.length == wlen), "writing to append file")

fs.closeSync(fd);

// test reading from a+ file
fd = fs.openSync('testfile.txt', 'a+');
var rbuf = new Buffer(4);
var rlen = fs.readSync(fd, rbuf, 0, 4, 0);
assert((rbuf.length == rlen), "read from a+ file")
assert((rbuf.toString('ascii') == "test"),
    "read correct data from a+ file: " + rbuf.toString('ascii'));

// test that read position was kept
rbuf = new Buffer(5);
rlen = fs.readSync(fd, rbuf, 0, 5, null);
assert((rbuf.length == rlen), "read from a+ file (again)")
assert((rbuf.toString('ascii') == "write"),
    "read correct data from a+ file: " + rbuf.toString('ascii'));

fs.closeSync(fd);

// test reading past end of file
fd = fs.openSync('testfile.txt', 'r');
rbuf = new Buffer(12);
// write known bytes into the end, these should not be overwritten
rbuf.writeUInt8(0xde, 9);
rbuf.writeUInt8(0xde, 10);
rbuf.writeUInt8(0xde, 11);
rlen = fs.readSync(fd, rbuf, 0, 12, 0);
assert((rlen < rbuf.length), "try reading past end of file");
assert((rbuf.readUInt8(9) == 0xde), "data[9] was unchanged");
assert((rbuf.readUInt8(10) == 0xde), "data[10] was unchanged");
assert((rbuf.readUInt8(11) == 0xde), "data[11] was unchanged");
// terminate the string so it can be compared
rbuf.writeUInt8(0x00, 9);
assert((rbuf.toString('ascii') == "testwrite"),
    "read data that existed in r file: " + rbuf.toString('ascii'));

// test write to read only file
assert.throws(function() {
    fs.writeSync(fd, new Buffer("dummy"), 0, 5, 0);
}, "tried writing to read only file");

fs.closeSync(fd);

// test null position parameter
fd = fs.openSync('testfile.txt', 'r');
rbuf = new Buffer(4);
rlen = fs.readSync(fd, rbuf, 0, 4, null);
assert((rlen == rbuf.length),
    "try reading with null position first: " + rbuf.length);
assert((rbuf.toString('ascii') == "test"),
    "read data correct with null position first: " + rbuf.toString('ascii'));

rbuf = new Buffer(5);
rlen = fs.readSync(fd, rbuf, 0, 5, null);
assert((rlen == rbuf.length),
    "try reading with null position (again): " + rbuf.length);
assert((rbuf.toString('ascii') == "write"),
    "read data correct with null position (again): " + rbuf.toString('ascii'));

rbuf = new Buffer(4);
rlen = fs.readSync(fd, rbuf, 0, 4, 0);
assert((rlen == rbuf.length),
    "try reading with 0 position (after null): " + rbuf.length);
assert((rbuf.toString('ascii') == "test"),
    "read data correct with 0 position (after null): " + rbuf.toString('ascii'));

fs.closeSync(fd);

// test write with only buffer
fd = fs.openSync('testfile.txt', 'w+');
wbuf = new Buffer("writetest");
wlen = fs.writeSync(fd, wbuf);
assert((wlen == wbuf.length), "write with just buffer: " + wlen);

rbuf = new Buffer(9);
rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);

assert((rlen == rbuf.length),
    "try reading (write with no length): " + rbuf.toString('ascii'));
assert((rbuf.toString('ascii') == "writetest"),
    "read data correct (write with no length): " + rbuf.toString('ascii'));

// test write with buffer + offset
wbuf = new Buffer("_____12345");
wlen = fs.writeSync(fd, wbuf, 5);
assert((wlen == 5), "write with just buffer: " + wlen);

rbuf = new Buffer(5);
rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);
assert((rlen == rbuf.length),
    "try reading (write with buf/off): " + rbuf.toString('ascii'));
assert((rbuf.toString('ascii') == "12345"),
    "read data correct (write with buf/off): " + rbuf.toString('ascii'));

// test write with buffer + offset + length
wbuf = new Buffer("_____123456789");
wlen = fs.writeSync(fd, wbuf, 5, 9);
assert((wlen == 9), "write with just buffer: " + wlen);

rbuf = new Buffer(9);
rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);
assert((rlen == rbuf.length),
    "try reading (write with buf/off/len): " + rbuf.toString('ascii'));
assert((rbuf.toString('ascii') == "123456789"),
    "read data correct (write with buf/off/len): " + rbuf.toString('ascii'));

// test write with buffer + offset + length + position
wbuf = new Buffer("_____test");
wlen = fs.writeSync(fd, wbuf, 5, 4, 5);
assert((wlen == 4), "write with just buffer: " + wlen);

rbuf = new Buffer(9);
rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);
assert((rlen == rbuf.length),
    "try reading (write with buf/off/len/pos): " + rbuf.toString('ascii'));
assert((rbuf.toString('ascii') == "12345test"),
    "read data correct (write with buf/off/len/pos): " + rbuf.toString('ascii'));

fs.closeSync(fd);

// open/close several files
var fd1 = fs.openSync('tf1.txt', 'w+');
fs.writeSync(fd1, new Buffer('tf1'));
var fd2 = fs.openSync('tf2.txt', 'w+');
fs.writeSync(fd2, new Buffer('tf2'));
var fd3 = fs.openSync('tf3.txt', 'w+');
fs.writeSync(fd3, new Buffer('tf3'));

var fd2_buf = new Buffer(3);
fs.readSync(fd2, fd2_buf, 0, 3, 0);
assert((fd2_buf.toString('ascii') == 'tf2'),
    "read from middle opened file: " + fd2_buf.toString('ascii'));

fs.closeSync(fd1);
fs.closeSync(fd3);

fd2_buf = new Buffer(3);
fs.readSync(fd2, fd2_buf, 0, 3, 0);
assert((fd2_buf.toString('ascii') == 'tf2'),
    "read from middle opened file (again): " + fd2_buf.toString('ascii'));

fs.closeSync(fd2);

fs.unlinkSync('tf1.txt');
fs.unlinkSync('tf2.txt');
fs.unlinkSync('tf3.txt');

fs.unlinkSync('testfile.txt');

// test that w overwrites existing file data
fd1 = fs.openSync('testfile.txt', 'w');
fs.writeSync(fd1, new Buffer('somedata'));
fs.closeSync(fd1);
fd1 = fs.openSync('testfile.txt', 'w+');
fs.writeSync(fd1, new Buffer('over'));
rbuf = new Buffer(8);
rlen = fs.readSync(fd1, rbuf, 0, 8, 0);
assert(rlen == 4, "length correct: " + rlen);
// null terminate at index 4
rbuf.writeUInt8(0x00, 4);
assert(rbuf.toString('ascii') == 'over',
    "file was overwritten with 'w': " + rbuf.toString('ascii'));
fs.closeSync(fd1);

fs.unlinkSync('testfile.txt');

assert.result();
