// Copyright (c) 2016, Intel Corporation.

// Testing UART APIs

console.log("test UART read data");

var board = require("uart");

var currentData = "";
var keyWords = ["hello", "WORLD", "^_^", "~!@#$%^&*()_+", "1234567890"];
var i = 0;
var keyWord = keyWords[0];
console.log("please send 'hello'");
board.init({ port: "tty0", baud: 115200 }).then(function (uart) {
    uart.setReadRange(1, 8);
    uart.on("read", function (data) {
        if (data.toString("ascii") === "\n" ||
            data.toString("ascii") === "\r") {
            if (i < keyWords.length) {
                console.log("uart.onread(data): expected result '" +
                            keyWord + "'");
            }

            console.log("received: " + currentData);
            uart.write(new Buffer("received: " + currentData + "\r\n"));

            if (i <= keyWords.length) {
                if (currentData === keyWord) {
                    i = i + 1;
                    if (i === keyWords.length) {
                        console.log("Testing completed");
                    } else {
                        keyWord = keyWords[i];
                        console.log("please send '" + keyWord + "'");
                    }
                }
            }

            currentData = "";
        } else {
            currentData += data.toString("ascii");
        }
    });
}).catch(function (error) {
    console.log("starting uart: " + error.name + " msg: " + error.message);
});
