var fs = require('fs');

var pass = true;

// Clean up any test files left from previous tests
var stats = fs.statSync('testfile.txt');
if (stats.isFile() || stats.isDirectory()) {
    fs.unlinkSync('testfile.txt');
    console.log('removing testfile.txt');
}


var fd = fs.openSync('testfile.txt', 'w');
var wbuf = new Buffer('bytes for writing');
var wlen = fs.writeSync(fd, wbuf, 0, wbuf.length, 0);
var rbuf = new Buffer(wbuf.length);
var rlen = fs.readSync(fd, rbuf, 0, rbuf.length, 0);

if (rbuf.toString('ascii') != wbuf.toString('ascii')) {
    console.log('error, buffers do not match; wbuf=' +
        wbuf.toString('ascii') + ' rbuf=' + rbuf.toString('ascii'));
    pass = false;
}


if (pass) {
    console.log('FS test passed');
} else {
    console.log('FS test failed');
}
