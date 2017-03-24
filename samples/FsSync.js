// Copyright (c) 2017, Intel Corporation.

var fs = require('fs');

console.log('File System sample');

var fd = fs.openSync('testfile.txt', 'w+');

var stats = fs.statSync('testfile.txt');

if (stats.isFile()) {
    console.log('fd is file');
} else if (stats.isDirectory()) {
    console.log('fd is directory');
} else {
    console.log('fd has unknown type');
}

var wbuf = new Buffer('write some bytes');

var written = fs.writeSync(fd, wbuf, 0, wbuf.length, 0);

console.log('wrote ' + written + ' bytes');


var rbuf = new Buffer(16);
var ret = fs.readSync(fd, rbuf, 0, 16, 0);
console.log('read ' + ret + ' bytes from file');
console.log('buffer=' + rbuf.toString('ascii'));

var fd1 = fs.openSync('file.txt', 'w');

fs.writeFileSync('string.txt', 'string data', {});
fs.writeFileSync('buffer.txt', new Buffer('buffer data'));

var files = fs.readdirSync('/');
for (var i = 0; i < files.length; ++i) {
    console.log('files[' + i + ']=' + files[i]);
}
