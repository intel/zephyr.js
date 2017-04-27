// Copyright (c) 2016-2017, Intel Corporation.

// Testing UART APIs

console.log("UART APIs test");

var board = require("uart");
var assert = require("Assert.js");

var e1 = board.init({ port: "value", baud: 115200 });
assert(typeof error === "object" && error !== null,
       "uart: catch error for UART.init");
assert(true, "uart: set port as value");

var e2 = board.init({ port: "tty0", baud: 9600 });
assert(true, "uart: set baud as 9600");

var uart = board.init({ port: "tty0", baud: 115200 });
assert(true, "uart: set init as tty0 and 115200");
assert(uart !== null && typeof uart === "object",
       "uart: callback value for UART.init");

uart.setReadRange(1, 16);
uart.on("read", function (data) {});

uart.write(new Buffer("UART write test\r\n"));
assert(true, "uart: write data with string");

uart.write(new Buffer("write: " + 1024 + "\r\n"));
assert(true, "uart: write data with number");

uart.write(new Buffer("write: " + true + "\r\n"));
assert(true, "uart: write data with boolean");

assert.result();

try {
	uart.write("write error value test\r\n");
	assert(false, "uart: write error value");
} catch (error) {
	assert(typeof error === "object" && !!error,
	"uart: write error value");
}
