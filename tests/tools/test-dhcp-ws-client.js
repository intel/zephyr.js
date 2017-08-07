// Copyright (c) 2017, Intel Corporation.

var WebSocket = require("ws");

var ws = new WebSocket("ws://[" + process.argv[2] + "]:8080", ["first", "testProtocol", "last"] );
var i = 0;

ws.on("open", function () {
    console.log("open WebSocket");

    ws.send("hello");
});

ws.on("message", function(data, flags) {
    console.log("Receive data: " + data);
    i = i + 1;

    if (i === 3) {
        ws.send("close");
        ws.close();
        console.log("close WebSocket");
    } else {
        ws.send("hello");
        console.log("Send data: hello");
    }
});

ws.on("close", function () {
    console.log("close WebSocket");
});

ws.on("error", function(error) {
    console.log("error: " + error.name + "  " + error.message);
});
