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
assert((rbuf.toString('ascii') == "test"), "read correct data from a+ file: " + rbuf.toString('ascii'));

// test that read position was kept
rbuf = new Buffer(5);
rlen = fs.readSync(fd, rbuf, 0, 5, 0);
assert((rbuf.length == rlen), "read from a+ file (again)")
assert((rbuf.toString('ascii') == "write"), "read correct data from a+ file: " + rbuf.toString('ascii'));

fs.closeSync(fd);

// test reading past end of file
fd = fs.openSync('testfile.txt', 'r');
rbuf = new Buffer(12);
rlen = fs.readSync(fd, rbuf, 0, 12, 0);
assert((rlen < rbuf.length), "try reading past end of file");
assert((rbuf.toString('ascii') == "testwrite"), "read data that existed in r file");

// test write to read only file
expectThrow("tried writing to read only file", function() {
    fs.writeSync(fd, new Buffer("dummy"), 0, 5, 0);
});

fs.closeSync(fd);

fs.unlinkSync('testfile.txt');

console.log("TOTAL: " + passed + " of " + total + " passed");