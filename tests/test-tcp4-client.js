// Copyright (c) 2017, Intel Corporation.

// Run this test case and run simple TCP server on linux
//     TCP server: /tests/tools/test-tcp4-server.py
// Add soft router on linux:
//     ip -4 route add 192.0.2/24 dev eno1
// Add IPv4 address on linux:
//     ip -4 addr add 192.0.2.2/24 dev eno1

console.log("Test socket connection as TCP client for IPv4");

var net = require("net");
var assert = require("Assert.js");

var IPv4Address = "192.0.2.2";
var IPv4Port = 9876;
var addressOptions = {
    port: IPv4Port,
    host: IPv4Address,
    localAddress: "192.0.2.1",
    localPort: 8484,
    family: 4
}

var socket = new net.Socket();

assert(typeof socket === "object" && socket !== null,
       "SocketObject: be defined for TCP client");

socket.on("data", function(buf) {
    console.log("receive data: " + buf.toString("ascii"));
});

var SocketconnectFlag = true;
socket.on("connect", function() {
    if (SocketconnectFlag) {
        assert(true, "SocketEvent: be called as 'connect' event");

        SocketconnectFlag = false;
    }

    console.log("socket connection is connected");

    var Count = 0;
    var Timer = setInterval(function() {
        if (Count < 7) {
            socket.write(new Buffer("hello\r\n"));

            console.log("send data: 'hello'");
        }

        if (Count === 7) {
            socket.write(new Buffer("close\r\n"));

            console.log("send data: 'close'");

            assert.result();

            clearInterval(Timer);
        }

        Count = Count + 1;
    }, 3000);
});

socket.on("close", function() {
    console.log("socket connection is closed");
});

var socketonErrorFlag = true;
socket.on("error", function(err) {
    if (socketonErrorFlag) {
        assert(true, "SocketEvent: be called as 'error' event");

        socketonErrorFlag = false;
    }

    console.log("socket connection error: " + err.name);

    if (err.name === "Error") {
        setTimeout(function() {
            socket.connect(addressOptions, function() {
                assert(true, "SocketObject: connect server successful");
            });
        }, 5000);
    }
});

console.log("Add soft router on linux:");
console.log("    ip -4 route add 192.0.2/24 dev eno1");
console.log("Add IPv4 address on linux:");
console.log("    ip -4 addr add 192.0.2.2/24 dev eno1");

console.log("TCP client will be connecting after 30s on K64f board");
setTimeout(function() {
    socket.connect(addressOptions, function() {
        assert(true, "SocketObject: connect server successful");
    });
}, 30000);
