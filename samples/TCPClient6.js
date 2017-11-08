// Copyright (c) 2017, Intel Corporation.
//
// Sample for "net" module implementing an IPv6 TCP echo client.
// To run it on the Arduino 101, you'll need to connect via BLE with your
// host machine (e.g. Linux), then add a new route for the bt0 interface:
//
// ip -6 route add 2001:db8::/64 dev bt0
//
// You can then run a TCP server (e.g. python) which this sample will connect
// to. You should then see it echo back any data send by the server.
//
// Note: There is some weirdness with network clients on Zephyr. There seems to
//       be an issue with trying to connect before the bluetooth interface has
//       come up. This is why we have the netup event. This sample also does
//       not issue the first connect until 10 seconds after you make the BLE
//       connection. This is done to give you enough time to setup the IP route
//       and start the TCP server on the host (e.g. Linux) side.

console.log("Starting TCPClient6.js sample...");

var net = require('net');
var net_cfg = require('netconfig');

var localIP = "2001:db8::1";
var localPort = 8484;
var remoteIP = "2001:db8::2";
var remotePort = 4242;

var connectOptions = {
    host: remoteIP,
    port: remotePort,
    localAddress: localIP,
    localPort: localPort,
    family: 6
};

net_cfg.setStaticIP('2001:db8::1');

net_cfg.on('netup', function() {
	console.log("network up");
	setTimeout(function() {
		console.log("starting client");
		var client = new net.Socket();

		var count = 0;
		var is_connected = false;

		function connected() {
			console.log("connect successful");
			is_connected = true;
			client.write(new Buffer("initial data\r\n"));
		}

		client.on("data", function(data) {
			count++;
			console.log("got data: " + data.toString('ascii'));
			client.write(new Buffer("write data " + count + "\r\n"));
		});
		client.on("close", function() {
			console.log("Socket has closed");
		});
		client.on("error", function(err) {
			if (err.name == "Error") {
				console.log("Server not found, retrying in 2 seconds");
				setTimeout(function() {
					if (!is_connected) {
						client.connect(connectOptions, connected);
					}
				}, 2000);
			}
		});

		console.log("Connecting...");
		client.connect(connectOptions, connected);
	}, 10000);
});
