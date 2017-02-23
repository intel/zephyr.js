// Copyright (c) 2017, Intel Corporation.

// Testing UDP Server APIs

console.log("test UDP Server APIs for ipv4");

var dgram = require("dgram");
var assert = require("Assert.js");

var PORT = 33333;
var HOST = "www.google.com";
var bindFlag = true;
var messageFlag = true;
var sendFlag = true;

var server = dgram.createSocket("udp4");
assert(server !== null && typeof server === "object",
       "udp4: defined udp4 server");

server.on("message", function(message, remote) {
    console.log(remote.address + ':' + remote.port +
                ' - ' + message.toString("ascii"));
    server.send(message, 0, message.length, remote.port, remote.address);

    if (bindFlag) {
        assert(true, "udp4: bind on " + HOST + " : " + PORT);

        bindFlag = false;
    }

    if (sendFlag) {
        assert.throws(function() {
            server.send(message, message.length + 1, message.length,
                        remote.port, remote.address);
        }, "send: set offset greater than length");

        assert.throws(function() {
            server.send(message, 0, -1, remote.port, remote.address);
        }, "send: set length as '-1'");

        assert.throws(function() {
            server.send(message, 0, message.length,
                        "value", remote.address);
        }, "send: set port as 'value'");

        assert.throws(function() {
            server.send(message, 0, message.length,
                        remote.port, "www.google.com");
        }, "send: set address as 'www.google.com'");

        sendFlag = false;
    }

    if (messageFlag) {
        assert(message !== null && typeof message === "object" &&
               remote !== null && typeof remote === "object",
               "udp4: callback value for server event 'message'");

        assert.result();

        messageFlag = false;
    }
});

server.on("error", function(err) {
    // In the current version, this callback is never called
    assert(err !== null && typeof err === "object",
           "udp4: callback value for server event 'error'");

    console.log("error: " + err);
});

assert.throws(function() {
    server.bind(PORT, HOST);
}, "udp4: set HOST as 'www.google.com'");

HOST = "192.0.2.10";
assert.throws(function() {
    server.bind(PORT, HOST);
}, "udp4: set HOST as '192.0.2.10'");

HOST = "192.0.2.1";
PORT = "value";
assert.throws(function() {
    server.bind(PORT, HOST);
}, "udp4: set PORT as 'value'");

PORT = 33333;
server.bind(PORT, HOST);

console.log("Please send some messages to " + HOST + " : " + PORT);
