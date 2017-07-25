// Copyright (c) 2016-2017, Intel Corporation.

// Testing UART APIs

console.log("Test UART read data");

var board = require("uart");

var current = "";
var keyWords = ["hello", "WORLD", "~!@#$%^&*()_+", "1234567890"];
var i = 0;
var keyWord = keyWords[i];

var uart = board.init({ port: "tty0", baud: 115200});
uart.setReadRange(1, 8);
uart.on("read", function (data) {
    if (data.toString("ascii") === "\n" ||
        data.toString("ascii") === "\r") {

        if (i < keyWords.length) {
            uart.write(new Buffer("Data received: " + current + " expected: " + keyWord + "\r\n"));
            if (current === keyWord) {
                i++;
                if (i === keyWords.length) {
                    uart.write(new Buffer("Testing completed\r\n"));
                } else {
                    keyWord = keyWords[i];
                    uart.write(new Buffer("please send '" + keyWord + "'\r\n"));
                }
            }
        } else {
            uart.write(new Buffer("Data received: " + current + "\r\n"));
        }
        current = '';
    } else {
        current += data.toString('ascii');
        uart.write(new Buffer(current + '\r\n'));
    }
});

uart.write(new Buffer("please send '" + keyWord + "'\r\n"));
