// Copyright (c) 2016, Intel Corporation.

// Sample to test UART module. This can be run on the Arduino 101 or QEMU. It
// will print out hello world to the console, then echo back any characters
// that are typed and store the string. If enter is pressed, it will print
// out the saved string and clear it.

var board = require('uart');

board.init({port:"tty0"}).then(function(uart) {
    var current = '';

    uart.setReadRange(1, 16);

    uart.on('read', function(data) {
        if (data.toString('ascii') == '\n' || data.toString('ascii') == '\r') {
            print("Data recv: " + current);
            current = '';
        } else {
            current += data.toString('ascii');
            print('\r' + current);
        }
    });
    uart.write(new Buffer('hello world'));
});
