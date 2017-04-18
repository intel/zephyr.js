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
//    server.send(message, 0, message.length, remote.port, remote.address);
});

server.bind(PORT, HOST);

console.log("Drop me a (UDP) packet at " + HOST + ":" + PORT);

var buf = new Buffer("foo");
server.send(buf, 0, buf.length, PORT, HOST);

console.log("End");
