print("Starting uart test");
var uart = require("uart");

pin = uart.open({port:"tty"});

var current = '';

pin.onread = function(data) {
    if (data == "\n" || data == "\r") {
        print("Data recv: " + current);
        current = '';
    } else {
        current += data;
        print("\r" + current);
    }
}

pin.write('hello world');

