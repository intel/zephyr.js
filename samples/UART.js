// Copyright (c) 2016, Intel Corporation.

// Sample to test UART module. This can be run on the Arduino 101 or QEMU. It
// will print out hello world to the console, then echo back any characters
// that are typed and store the string. If enter is pressed, it will print
// out the saved string and clear it.
// On the Arduino 101, this sample can be used by hooking up a USB-to-serial
// device to the RX and TX lines (pins 0 and 1 respectively).

print("Starting UART example");

var board = require('uart');

var readBuf = new Buffer(128);
var pos = 0;

board.init({port:"tty0", baud:115200}).then(function(uart) {
    uart.setReadRange(1, 16);

    uart.on('read', function(data) {
        if (data.toString('ascii') == '\n' || data.toString('ascii') == '\r') {
            uart.write(new Buffer("Data: "));
            uart.write(readBuf);
            pos = 0;
        } else {
            readBuf.write(data.toString('ascii') + '\n', pos++, data.length + 1, "utf8");
            uart.write(readBuf);
        }
    });
    uart.write(new Buffer('hello world\n'));
}).catch(function(error) {
    print("Error starting UART: " + error.name + " msg: " + error.message);
});
