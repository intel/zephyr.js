// Copyright (c) 2017, Intel Corporation.

// Sample for "ws" module implementing a Web Socket server.
// Same setup should be followed as the 'net' module for IPv4 with DHCP.
// Refer to samples/TCPEchoServ4.js

// Then you can use a websocket client samples/websockets/NodeWebsocketClient4.js
// to connect and interact with the server.

var WebSocket = require('ws');
var net_cfg = require('net_config');

console.log("starting DHCP");
net_cfg.dhcp(function(address, subnet, gateway) {
    console.log("Got address: " + address);
    var wss = new WebSocket.Server({
        port: 8080,
        host: '0.0.0.0',
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
                ws.send(new Buffer("A MESSAGE"), true);
                if (count % 10 == 0) {
                    ws.ping(new Buffer("PING"));
                }
                count++;
            }, 1000);
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
});
