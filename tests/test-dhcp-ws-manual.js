// Copyright (c) 2017, Intel Corporation.

console.log("Test DHCP APIs");

var WebSocket = require("ws");
var netConfig = require("netconfig");
var board = require("board");

var IPState;
var IPv6 = "2001:db8::100";
var IPv4 = "192.168.1.150";
var Port = 8080;

var ServerHandler = function(protos) {
    for (var i = 0; i < protos.length; i++) {
        if (protos[i] === "testProtocol") {
            return protos[i];
        }
    }

    return false;
}

var SocketBody = function(server) {
    server.on("connection", function(websocket) {
        console.log("\nCreate web socket connection successful");

        websocket.on("message", function(message) {
            console.log("Receive data: " + message.toString("ascii"));

            if (message.toString("ascii") === "close") {
                if (board.name === "arduino_101") {
                    console.log("\nPlease close ws client " +
                                "and disconnect a101 with Bluetooth....");
                } else if (board.name === "frdm_k64f") {
                    console.log("\nPlease close ws client");
                }
            } else {
                websocket.send(new Buffer("DHCP"), true);
                console.log("Send data: DHCP");
            }
        });

        websocket.on("close", function(code, reason) {
            console.log("Closed: " + code + " : " + reason);
            if (board.name === "frdm_k64f") {
                console.log("Testing completed");
            }
        });

        websocket.on("error", function(error) {
            console.log("Error: " + error.name + " : " + error.message);
        });
    });
}

if (board.name === "arduino_101") {
    var bleAddr = "F1:E2:D3:C4:B5:A6";
    netConfig.setBleAddress(bleAddr);

    console.log("\nPlease connect a101 with Bluetooth....");
    console.log("setBleAddress(bleAddr): expected result 'F1:E2:D3:C4:B5:A6'\n");

    netConfig.on("netup", function() {
        console.log("'onnetup' Event: expected result 'Connected'\n");

        try {
            IPState = netConfig.setStaticIP(IPv4);
        } catch (e) {
            console.log("setStaticIP(IPv4): misconfiguration " +
                        "expected result 'error'");
            console.log("    " + e.name + " : " + e.message);
        }

        IPState = netConfig.setStaticIP(IPv6);
        console.log("setStaticIP(IPv6): '" + IPState +
                    "' expected result 'true'\n");

        console.log("Add soft router on linux:");
        console.log("    ip -6 route add 2001:db8::/64 dev bt0");
        console.log("Add IPv6 addr on linux:");
        console.log("    ip -6 addr add 2001:db8::2/64 dev bt0");
        console.log("Run ws client on linux by /tools/test-dhcp-ws-client.js");
        console.log("    node tools/test-dhcp-ws-client.js " + IPv6);

        var WSServer = new WebSocket.Server({
            port: Port,
            backlog: 5,
            clientTracking: true,
            maxPayload : 125,
            acceptHandler: ServerHandler
        });

        SocketBody(WSServer);

        WSServer.on('error', function (error) {
            console.log('Server socket: ' + error.name + ': ' + error.message);
        });
    });

    netConfig.on("netdown", function() {
        console.log("'onnetdown' Event: expected result 'Disconnected'\n");
        console.log("Testing completed");
    });
} else if (board.name === "frdm_k64f") {
    console.log("\nPlease connect K64f with route....\n");

    netConfig.dhcp(function(address, subnet, gateway) {
        console.log("DHCP IPv4 address: " + address +
                    " expected result 'valid value'");
        console.log("DHCP IPv4 subnet: " + subnet +
                    " expected result 'valid value'");
        console.log("DHCP IPv4 gateway: " + gateway +
                    " expected result 'valid value'");

        console.log("\nRun ws client on linux by /tools/test-dhcp-ws-client.js");
        console.log("    node tools/test-dhcp-ws-client.js " + address);

        var WSServer = new WebSocket.Server({
            host: "0.0.0.0",
            port: Port,
            backlog: 5,
            clientTracking: true,
            maxPayload : 125,
            acceptHandler: ServerHandler
        });

        SocketBody(WSServer);

        WSServer.on('error', function (error) {
            console.log('Server socket: ' + error.name + ': ' + error.message);
        });
    });
}
