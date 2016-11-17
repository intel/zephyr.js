// Copyright (c) 2016, Intel Corporation.

// Sample to test UART module. This can be run on the Arduino 101 or QEMU. It
// will print out 'UART write succeeded, echoing input.' to the console, then echo back any characters
// that are typed and store the string. If enter is pressed, it will print
// out the saved string and clear it.

// On the Arduino 101, this sample can be used by opening /dev/ttyACMX using
// screen or minicom:
//   $ screen /dev/ttyACM0 115200
//   $ minicom --device /dev/ttyACM0,

// The ACM port just uses the Arduino 101's USB port, so the only wiring needed
// is to connect that USB port to the computer, and ensure you see an ACM port
// once the A101 boots up.

// Hardware Requirements:
//   - Arduino 101
//   - USB A/B cable
// Wiring:
//   - Connect the USB cable to the Arduino 101 and your host PC

print("Starting UART example...");

var board = require('uart');

board.init({ port:"tty0", baud:115200 }).then(function(uart) {
    var current = '';

    uart.setReadRange(1, 16);

    uart.on('read', function(data) {
        if (data.toString('ascii') == '\n' || data.toString('ascii') == '\r') {
            uart.write(new Buffer("Data recv: " + current + '\r\n'));
            current = '';
        } else {
            current += data.toString('ascii');
            uart.write(new Buffer(current + '\r\n'));
        }
    });
    uart.write(new Buffer('UART write succeeded, echoing input.\r\n'));
}).catch(function(error) {
    print("Error starting UART: " + error.name + " msg: " + error.message);
});
