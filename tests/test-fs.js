var fs = require('fs');

var total = 0;
var passed = 0;
var pass = true;

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

// Clean up any test files left from previous tests
var stats = fs.statSync('testfile.txt');
if (stats.isFile() || stats.isDirectory()) {
    fs.unlinkSync('testfile.txt');
    console.log('removing testfile.txt');
}

// test file creation
var fd = fs.openSync('testfile.txt', 'a');
fs.closeSync(fd);
var fd_stats = fs.statSync('testfile.txt');
var success = false;
if (fd_stats.isFile()) {
	success = true;
}
assert(success, "create file");

// test file removal
fs.unlinkSync('testfile.txt');
fd_stats = fs.statSync('testfile.txt');
success = false;
if (!fd_stats.isFile()) {
	success = true;
}
assert(success, "remove file");

//test invalid file open (on a non-existing file)
expectThrow("open(r) on non-existing file", function () {
    fd = fs.openSync('testfile.txt', 'r');
});

expectThrow("open(r+) on non-existing file", function () {
    fd = fs.openSync('testfile.txt', 'r+');
});

// Test appending options
fs.writeFileSync("testfile.txt", new Buffer("test"));

fd = fs.openSync('testfile.txt', 'a');

// test reading from 'a' file
expectThrow("can't read from append file", function() {
    var rbuf = new Buffer(wbuf.length);
    var rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);
});

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
rlen = fs.readSync(fd, rbuf, 0, 12, 0);
assert((rlen < rbuf.length), "try reading past end of file");
assert((rbuf.toString('ascii') == "testwrite"),
    "read data that existed in r file: " + rbuf.toString('ascii'));

// test write to read only file
expectThrow("tried writing to read only file", function() {
    fs.writeSync(fd, new Buffer("dummy"), 0, 5, 0);
});

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

console.log("TOTAL: " + passed + " of " + total + " passed");
