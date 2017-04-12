// Copyright (c) 2017, Intel Corporation.

var net = require('net');

var client = new net.Socket();

var count = 0;

function connected() {
	console.log("connect successful");
	//client.write(new Buffer("initial data"));
}

client.on("data", function(data) {
	console.log("got data: " + data.toString('ascii'));
	client.write(new Buffer("write data " + count++));
});
client.on("close", function() {
	console.log("Socket has closed");
});
client.on("error", function(err) {
	if (err.name == "NotFoundError") {
		console.log("Server not found, retrying in 2 seconds");
		setTimeout(function() {
			if (!client.connecting) {
				client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
			}
		}, 2000);
	}
});

console.log("Connecting");
client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
