// Copyright (c) 2017, Intel Corporation.

// CMD: node test-ws4-client-dhcp.js <DHCP Server IP>

var WebSocket = require("ws");

console.log("Opening connection to: " + process.argv[2] + ":8080");
var ws = new WebSocket("ws://" + process.argv[2] + ":8080",
                       ["first", "testProtocol", "last"] );

ws.on("open", function() {
    console.log("open WebSocket");

    ws.send("hello world");
});

ws.on("message", function(data, flags) {
    console.log("Receive: " + data);
    if (data.toString("ascii") === "close") {
        ws.close();
    } else {
        ws.send(data);
        console.log("Send: " + data);
    }
});

ws.on("ping", function(data, flags) {
    console.log("Receive 'ping' and send 'pong': " + data);
});

var testPong = true;
var pongTest = true;
ws.on("pong", function(data, flags) {
    if (testPong && data.toString("ascii") === "testPong") {
        ws.ping(data);

        testPong = false;
    }

    if (pongTest && data.toString("ascii") === "pongTest") {
        ws.ping(data);

        pongTest = false;
    }

    console.log("Receive 'pong' and send 'ping': " + data);
});

ws.on("close", function() {
    console.log("close WebSocket");
});

ws.on("error", function(error) {
    console.log("error: " + error.name + "  " + error.message + "  " + JSON.stringify(error));
});
