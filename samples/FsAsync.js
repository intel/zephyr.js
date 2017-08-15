// Copyright (c) 2017, Intel Corporation.

var fs = require('fs');

console.log('File System sample');

fs.open('testfile.txt', 'w+', function(err, fd) {
    console.log('opened file error=' + err);
    fs.stat('testfile.txt', function(err, stats) {
        if (stats && stats.isFile()) {
            console.log('fd is file');
        } else if (stats && stats.isDirectory()) {
            console.log('fd is directory');
        } else {
            console.log('fd has unknown type');
        }
    });
    var wbuf = new Buffer('write some bytes');
    fs.write(fd, wbuf, 0, wbuf.length, 0, function(err, written, buffer) {
        console.log('wrote ' + written + ' bytes');
        var rbuf = new Buffer(16);
        fs.read(fd, rbuf, 0, 16, 0, function(err, num, buf) {
            console.log('read from file; error=' + err);
            console.log('buffer=' + buf.toString('ascii'));
        });
    });
});

fs.open('file.txt', 'w', function(err, fd) {
    console.log('opened file error=' + err);
});

fs.writeFile('string.txt', 'string data', function(err) {
    console.log('wrote string data file error=' + err);
});

fs.writeFile('buffer.txt', new Buffer('buffer data'), function(err) {
    console.log('wrote buffer data file error=' + err);
});

setTimeout(function() {
    fs.readdir('/', function(err, files) {
        for (var i = 0; i < files.length; ++i) {
            console.log('files[' + i + ']=' + files[i]);
        }
    });
}, 1000);
