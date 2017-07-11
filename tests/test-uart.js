// Copyright (c) 2016-2017, Intel Corporation.

// Testing UART APIs

console.log("UART APIs test");

var board = require("uart");
var assert = require("Assert.js");


var t1 = board.init({ port: "invalid", baud: 115200 });
assert(!("write" in t1), "UART: set port with invalid value");

var t2 = board.init({ port: "tty0", baud: 115200 });
assert("write" in t2, "UART: set baud as 9600");

var t3 = board.init({ port: "tty0", baud: 115200 });
assert("write" in t3 && typeof t3.write == "function",
       "UARTConnection: has write method");
assert("setReadRange" in t3 && typeof t3.setReadRange == "function",
       "UARTConnection: has setReadRange method");

assert.throws(function () {
    t3.write("Not a buffer\r\n");
}, "UARTConnection: write error value");

assert.result();

