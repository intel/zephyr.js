// Copyright (c) 2017, Intel Corporation.

// Run this test case and run ws client on linux by /tools/test-ws4-client-dhcp.js

console.log("Test web socket server APIs about IPv4 with DHCP mode");

var WebSocket = require("ws");
var assert = require("Assert.js");
var netConfig = require("net-config");

netConfig.dhcp(function(address, subnet, gateway) {
    console.log("IPv4 address: " + address);

    var ServerHandler = function(protos) {
        for (var i = 0; i < protos.length; i++) {
            if (protos[i] == "testProtocol") {
                return protos[i];
            }
        }

        return false;
    }

    var WSServer = new WebSocket.Server({
        host: "0.0.0.0",
        port: 8080,
        backlog: 5,
        clientTracking: true,
        maxPayload : 125,
        acceptHandler: ServerHandler
    });

    assert(typeof WSServer === "object" && WSServer !== null,
           "WSServerObject: be defined");

    WSServer.on("connection", function(websocket) {
        assert(true, "WSServerObject: accept client connected");
        assert(true, "WSServerEvent: be called as 'connection' event");

        console.log("Creat web socket connection successful");

        var messageFlag = true;
        var sendNoMaskFlag = false;
        var sendMaskFlag = false;
        websocket.on("message", function(message) {
            if (messageFlag) {
                assert(true, "WebSocketEvent: be called as 'message' event");
                assert(true, "WebSocketObject: receive data from client as '" +
                       message.toString("ascii") + "'");

                messageFlag = false;
            }

            if (sendNoMaskFlag) {
                assert(message.toString("ascii") === "hello",
                       "WebSocketObject: send data without mask");

                sendNoMaskFlag = false;
            }

            if (sendMaskFlag) {
                assert(message.toString("ascii") === "world",
                       "WebSocketObject: send data with mask");

                sendMaskFlag = false;
            }

            console.log("ReceiveData: " + message.toString("ascii"));
        });

        var pingFlag = true;
        var pingNoMaskFlag = false;
        var pingMaskFlag = false;
        websocket.on("ping", function(data) {
            if (pingFlag) {
                assert(true, "WebSocketEvent: be called as 'ping' event");

                pingFlag = false;
            }

            if (pingNoMaskFlag) {
                assert(data.toString("ascii") === "pongTest",
                       "WebSocketObject: pong and ping data without mask");

                pingNoMaskFlag = false;
            }

            if (pingMaskFlag) {
                assert(data.toString("ascii") === "testPong",
                       "WebSocketObject: pong and ping data with mask");

                pingMaskFlag = false;
            }

            console.log("ReceivePing: " + data.toString("ascii"));
        });

        var pongFlag = true;
        var pongNoMaskFlag = false;
        var pongMaskFlag = false;
        websocket.on("pong", function(data) {
            if (pongFlag) {
                assert(true, "WebSocketEvent: be called as 'pong' event");

                pongFlag = false;
            }

            if (pongNoMaskFlag) {
                assert(data.toString("ascii") === "pingTest",
                       "WebSocketObject: ping and pong data without mask");

                pongNoMaskFlag = false;
            }

            if (pongMaskFlag) {
                assert(data.toString("ascii") === "testPing",
                       "WebSocketObject: ping and pong data with mask");

                pongMaskFlag = false;
            }

            console.log("ReceivePong: " + data.toString("ascii"));
        });

        var closeFlag = true;
        websocket.on("close", function(code, reason) {
            if (closeFlag) {
                assert(true, "WebSocketEvent: be called as 'close' event");

                assert.result();
                closeFlag = false;
            }

            console.log("ReceiveClose: " + code + " : " + reason);
        });

        var errorFlag = true;
        websocket.on("error", function(error) {
            if (errorFlag) {
                assert(true, "WebSocketEvent: be called as 'error' event");

                errorFlag = false;
            }

            console.log("ReceiveError: " + error.name + " : " + error.message);
        });

        var Count = 0;
        var Timer = setInterval(function () {
            if (Count === 1) {
                websocket.send(new Buffer("hello"));

                console.log("SendData: hello");
                sendNoMaskFlag = true;
            }

            if (Count === 2) {
                websocket.send(new Buffer("world"), true);

                console.log("SendData: world");
                sendMaskFlag = true;
            }

            if (Count === 3) {
                websocket.ping(new Buffer("pingTest"));

                console.log("PingData: pingTest");
                pongNoMaskFlag = true;
            }

            if (Count === 4) {
                websocket.ping(new Buffer("testPing"), true);

                console.log("PingData: testPing");
                pongMaskFlag = true;
            }

            if (Count === 5) {
                websocket.pong(new Buffer("pongTest"));

                console.log("PongData: pongTest");
                pingNoMaskFlag = true;
            }

            if (Count === 6) {
                websocket.pong(new Buffer("testPong"), true);

                console.log("PongData: testPong");
                pingMaskFlag = true;
            }

            if (Count === 7) {
                websocket.send(new Buffer("close"));

                console.log("SendData: close");
            }

            Count = Count + 1;

            if (Count === 8) {
                clearInterval(Timer);
            }
        }, 3000);
    });
});
