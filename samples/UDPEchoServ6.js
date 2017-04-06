// Copyright (c) 2017, Linaro Limited.

// Sample for "dgram" module implementing an IPv6 UDP echo server
// and showing use of offset and length parameters to send() call,
// and error callback.

// To run it on a BLE/6lowpan device (A101/nRF52) you will need
// to first connect to the device using BLE, then add a new IP
// route to the bt0 interface:

// ip -6 route add 2001:db8::/64 dev bt0

// To run it on QEMU, you'll need additional
// setup for networking to work, follow
// https://www.zephyrproject.org/doc/subsystems/networking/qemu_setup.html

// From the host, you can generate test UDP traffic using "netcat"
// tool:
//
// nc -u -6 2001:db8::1 33333
//
// (Then start typing and pressing Enter).

var PORT = 33333;
var HOST = '2001:db8::1';

var dgram = require('dgram');
var server = dgram.createSocket('udp6');

server.on('message', function (message, remote) {
    console.log(remote.address + ':' + remote.port +' - ' + message.toString("ascii"));
    server.send(message, 1, message.length - 1, remote.port, remote.address);
});

server.on('error', function (err) {
    console.log("error: " + err);
});

server.bind(PORT, HOST);

console.log("Drop me a (UDP) packet at " + HOST + ":" + PORT + ", I'll eat its first char");
