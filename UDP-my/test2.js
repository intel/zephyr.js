// Copyright (c) 2017, Linaro Limited.

// Sample for "dgram" module implementing an IPv4 UDP echo server.
// To run it on QEMU, you'll need additional setup for
// networking to work, follow
// https://www.zephyrproject.org/doc/subsystems/networking/qemu_setup.html
// From the host, you can generate test UDP traffic using "netcat"
// tool:
//
// nc -u 192.0.2.1 33333
//
// (Then start typing and pressing Enter).


var PORT = 33333;
var HOST = '192.0.2.1';

var dgram = require('dgram');

var server = dgram.createSocket('udp4');
server.on('message', function (message, remote) {
    console.log(remote.address + ':' + remote.port +' - ' + message.toString("ascii"));
});
server.bind(PORT, HOST);

var client = dgram.createSocket('udp4');
var buf = new Buffer("foo");
client.send(buf, 0, buf.length, PORT, HOST);

console.log("End");
