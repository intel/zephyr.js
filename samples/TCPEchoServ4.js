// Copyright (c) 2017, Intel Corporation.

// Sample for "net" module implementing an IPv6 TCP echo server.
// To run it on the FRDM_K64F 101, you'll need to add a new route
// for its IP (default 192.0.2.0)
//
// ip route add 192.0.2.0 dev <ethX>
//
// Then you can initiate a TCP connection and see the data you sent being
// echo'ed back:
//
// echo "hello" | nc 192.0.2.1 4242

console.log("Starting TCPEchoServ4.js sample...");

var net = require('net');

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

server.listen({port: 4242, host: "0.0.0.0", family: 4}, function() {
    console.log("listening");
});

server.on("close", function() {
    console.log("SERVER HAS CLOSED");
});
