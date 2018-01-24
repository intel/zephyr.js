// Copyright (c) 2017, Intel Corporation.

// Sample for "ws" module implementing a Web Socket server.
// To run it on the Arduino 101, you'll need to first connect via BLE then
// add the IP route for the Arduino's 6lowpan address
//
// ip -6 route add 2001:db8::/64 dev bt0
//
// Then you can use a websocket client samples/websockets/NodeWebsocketClient.js
// to connect and interact with the server.

console.log("WebSocketServer sample...");

var WebSocket = require('ws');

var wss = new WebSocket.Server({
    port: 8080,
    acceptHandler: function(protos) {
        for (var i = 0; i < protos.length; i++) {
            if (protos[i] == "my_ws_protocol") {
                return protos[i];
            }
        }
        return false;
    }
});

var count = 0;
var t = null;

wss.on('connection', function(ws) {
    console.log("CONNECTION");
    ws.on('message', function(message) {
        console.log("MESSAGE: " + message.toString('ascii'));
        t = setTimeout(function() {
            ws.send(new Buffer("Message #" + count), true);
            if (count % 10 == 0) {
                ws.ping(new Buffer("PING"));
            }
            count++;
        }, 100);
    });
    ws.on('ping', function(data) {
        console.log("PING: " + data.toString('ascii'));
        ws.pong(data);
    });
    ws.on('pong', function(data) {
        console.log("PONG: " + data.toString('ascii'));
    });
    ws.on('close', function(code, reason) {
        console.log("close event: " + reason + " code: " + code);
        clearTimeout(t);
    });
    ws.on('error', function(error) {
        console.log(error.name + " : " + error.message);
        clearTimeout(t);
    });
});
