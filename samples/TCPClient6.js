// Copyright (c) 2017, Intel Corporation.

var net = require('net');

net.on('up', function() {
	console.log("network up");
	setTimeout(function() {
		console.log("starting client");
		var client = new net.Socket();

		var count = 0;
		var is_connected = false;

		function connected() {
			console.log("connect successful");
			is_connected = true;
			client.write(new Buffer("initial data"));
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
					if (!is_connected) {
						client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
					}
				}, 2000);
			}
		});

		console.log("Connecting");
		client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
	}, 10000);
});
