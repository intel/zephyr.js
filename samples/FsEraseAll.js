// Copyright (c) 2017, Intel Corporation.

var fs = require('fs');

console.log('Sample to erase all files on the file system');

function rmDirectory(dirPath) {
    var files = fs.readdirSync(dirPath);
    for (var i = 0; i < files.length; ++i) {
        var filePath = dirPath + '/' + files[i];
        var stats = fs.statSync(filePath);
        if (stats) {
            if (stats.isFile()) {
                fs.unlinkSync(filePath);
            } else if (stats.isDirectory()) {
                fs.rmdirSync(filePath);
            }
            console.log('Removed ' + files[i]);
        }
    }
    fs.rmdirSync(dirPath);
}

rmDirectory('/');
console.log('Removed all files');
