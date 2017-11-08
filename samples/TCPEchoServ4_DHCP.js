// Copyright (c) 2017, Intel Corporation.

// Sample for "net" module implementing an IPv4 TCP echo server.
// This sample has been tested using the FRDM-K64F board, with its ethernet
// connected to a router which assigns a DHCP address. You can then use a TCP
// client (e.g. Linux netcat) which on the same network as the router.

// If the Linux desktop's default interface is connected to the router, you
// should be able to run this sample and connect to the FRDM-K64F using netcat.
// If you are using a secondary interface you will need to add a route

// sudo ip route add 192.168.1/24 dev ethX

// Where "ethX" is the network interface on the same subnet as the FRDM-K64F

// You should then be able to start this sample, wait for "Got IP: <ip>" to
// print, then you can initiate a TCP connection and see the data you send being
// echo'ed back:

// echo "hello" | nc <DHCP ip> 4242

console.log("Starting TCPEchoServ4_DHCP.js sample...");

var net = require('net');
var net_cfg = require('netconfig');

net_cfg.dhcp(function(address, subnet, gateway) {
    console.log("Got IP: " + address);

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
});
