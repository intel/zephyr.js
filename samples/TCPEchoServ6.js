// Copyright (c) 2017, Intel Corporation.

// Sample for "net" module implementing an IPv6 TCP echo server.
// To run it on the Arduino 101, you'll need to connect via BLE with your
// TCP client (e.g. Linux netcat), then add a new route for the bt0 interface:
//
// ip -6 route add 2001:db8::/64 dev bt0
//
// Then you can initiate a TCP connection and see the data you sent being
// echo'ed back:
//
// echo "hello" | nc -6 2001:db8::1 4242

var net = require('net');

var socket = null;

var connections = 0;

var server = net.createServer(function(sock) {
    console.log("connection made");
    var info = sock.address();
    console.log("port: " + info.port);
    console.log("family: " + info.family);
    console.log("address: " + info.address);
    console.log("socket.localPort: " + sock.localPort);
    console.log("socket.localAddress: " + sock.localAddress);
    console.log("socket.remotePort: " + sock.remotePort);
    console.log("socket.remoteAddress: " + sock.remoteAddress);
    console.log("socket.remoteFamily: " + sock.remoteFamily);

    sock.on("data", function(buf) {
        console.log("Got data: " + buf.toString('ascii'));
        sock.write(buf);
    });
    sock.on("close", function() {
        console.log("Socket closed");
    });
});

server.listen({port: 4242, host: '::', family: 6}, function() {
    console.log("listening");
});

server.on("close", function() {
    console.log("SERVER HAS CLOSED");
});
