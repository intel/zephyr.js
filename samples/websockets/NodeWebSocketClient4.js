// Copyright (c) 2017-2018, Intel Corporation.

var WebSocket = require('ws');

console.log("opening connection to: " + process.argv[2] + ":" + process.argv[3]);
var ws = new WebSocket('ws://' + process.argv[2] + ":" + process.argv[3], ["first", "my_ws_protocol", "last"] );
ws.on('connection', function(s) {
    console.log("CONNECTION");
});
ws.on('open', function open() {
  ws.send('first packet');
});

ws.on('message', function(data, flags) {
  console.log(data);
  ws.send(data);
});

ws.on('ping', function(data, flags) {
    console.log("PING: " + data);
});

ws.on('error', function(error) {
	console.log("error: " + error.name + "  " + error.message + "  " + JSON.stringify(error));
});
