// Copyright (c) 2017, Intel Corporation.

var net = require('net');

<<<<<<< 72a9306aea9aa9d0f65b1360dc620f3fe1033931
<<<<<<< 26dd930f71b0ed14a5ef3194389be1d5ebfe838a
=======
>>>>>>> Addressed GH comments
net.on('up', function() {
	console.log("network up");
	setTimeout(function() {
		console.log("starting client");
		var client = new net.Socket();
<<<<<<< 72a9306aea9aa9d0f65b1360dc620f3fe1033931

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
=======
var client = new net.Socket();
=======
>>>>>>> Addressed GH comments

		var count = 0;
		var is_connected = false;

<<<<<<< 72a9306aea9aa9d0f65b1360dc620f3fe1033931
<<<<<<< a0ec723ee3c51e8e4ae96905270feccca0a9a6bd
setTimeout(function() {
	console.log("Connecting");
	client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, function() {
	    console.log("connect successful");
	    client.write(new Buffer("initial data"));
	});
	client.on("data", function(data) {
		console.log("got data: " + data.toString('ascii'));
		client.write(new Buffer("write data " + count++));
	});
	client.on("close", function() {
		console.log("Socket has closed");
	});
}, 10000);
>>>>>>> Client support
=======
function connected() {
	console.log("connect successful");
	//client.write(new Buffer("initial data"));
}
=======
		function connected() {
			console.log("connect successful");
			is_connected = true;
			client.write(new Buffer("initial data"));
		}
>>>>>>> Addressed GH comments

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

<<<<<<< 72a9306aea9aa9d0f65b1360dc620f3fe1033931
console.log("Connecting");
client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
>>>>>>> [fix connect]
=======
		console.log("Connecting");
		client.connect({port: 4242, host:'fe80::5ef3:70ff:fe78:1080', localAddress:'2001:db8::1', localPort: 8484, family: 6}, connected);
	}, 10000);
});
>>>>>>> Addressed GH comments
