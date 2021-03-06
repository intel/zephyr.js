// Copyright (c) 2017-2018, Intel Corporation.

// Run this test case and run simple TCP client on linux
//     TCP client: /tests/tools/test-tcp6-client.py
// If on a101 using Bluetooth communication, please connect client with Bluetooth:
//     Add soft router on linux:
//         ip -6 route add 2001:db8::/64 dev bt0
// If on K64f:
//     Add soft router on linux:
//         ip -6 route add 2001:db8::/64 dev eno1
//     Add IPv6 address on linux:
//         ip -6 addr add 2001:db8::2/64 dev eno1

console.log("Test TCP server for IPv6 and socket connection");

var net = require("net");
var assert = require("Assert.js");

var IPv4Address = "192.0.2.1";
var IPv6Address = "2001:db8::1";
var noIPAddress = "InvalidIP";
var IPv6Port = 9876;
var IPv6Family = 6;
var IPResult1, IPResult2, IPResult3;
var noIPAddresses = [
    1024,
    true,
    null,
    "",
    "192.168.0.2017",
    "2001:db8::true"
];

IPResult1 = net.isIP(IPv4Address);
assert(typeof IPResult1 === "number" && IPResult1 === 4,
       "IPAddress: check IP address as 'IPv4'");

IPResult1 = net.isIP(IPv6Address);
assert(typeof IPResult1 === "number" && IPResult1 === 6,
       "IPAddress: check IP address as 'IPv6'");

IPResult1 = net.isIP(noIPAddress);
assert(typeof IPResult1 === "number" && IPResult1 === 0,
       "IPAddress: check IP address as 'InvalidIP'");

IPResult1 = net.isIPv4(IPv4Address);
IPResult2 = net.isIPv4(IPv6Address);
IPResult3 = net.isIPv4(noIPAddress);
assert(typeof IPResult1 === "boolean" && IPResult1 === true &&
       typeof IPResult2 === "boolean" && IPResult2 === false &&
       typeof IPResult3 === "boolean" && IPResult3 === false,
       "IPAddress: check IPv4 address");

IPResult1 = net.isIPv6(IPv4Address);
IPResult2 = net.isIPv6(IPv6Address);
IPResult3 = net.isIPv6(noIPAddress);
assert(typeof IPResult1 === "boolean" && IPResult1 === false &&
       typeof IPResult2 === "boolean" && IPResult2 === true &&
       typeof IPResult3 === "boolean" && IPResult3 === false,
       "IPAddress: check IPv6 address");

for (var i = 0; i < noIPAddresses.length; i++) {
    IPResult1 = net.isIP(noIPAddresses[i]);
    assert(typeof IPResult1 === "number" && IPResult1 === 0,
           "IPAddress: check IP address as '" + noIPAddresses[i] + "'");

    IPResult2 = net.isIPv4(noIPAddresses[i]);
    assert(typeof IPResult2 === "boolean" && IPResult2 === false,
           "IPAddress: check IPv4 address as '" + noIPAddresses[i] + "'");

    IPResult3 = net.isIPv6(noIPAddresses[i]);
    assert(typeof IPResult3 === "boolean" && IPResult3 === false,
           "IPAddress: check IPv6 address as '" + noIPAddresses[i] + "'");
}

var serverA = net.createServer();

assert(typeof serverA === "object" && serverA !== null,
       "ServerObject: server is defined without callback function");

var listeningAFlag = serverA.listening;
serverA.listen({port: 4242, host: IPv6Address, family: IPv6Family});

serverA.on("listening", function() {
    assert(true, "ServerObject: begin to listen without callback function");
});

serverA.on("close", function() {
    assert(true, "ServerEvent: be called as 'close' event");
});

setTimeout(function() {
    var listeningBFlag = serverA.listening;

    serverA.getConnections(function(error, count) {
        assert(typeof count === "number" && count === 0,
               "ServerObject: get default connection as '0'");
    });

    var serverMaxCon = serverA.maxConnections;
    assert(typeof serverMaxCon === "number" && serverMaxCon === 5,
           "ServerObject: 'server.maxConnections' is defined as '5'");

    assert(typeof listeningAFlag === "boolean" && listeningAFlag === false &&
           typeof listeningBFlag === "boolean" && listeningBFlag === true,
           "ServerObject: 'server.listening' is defined");

    serverA.close();
}, 2000);

setTimeout(function() {
    var server = net.createServer(function(sock) {
        assert(typeof sock === "object" && sock !== null,
               "SocketObject: be defined for socket connected");

        var info = sock.address();
        console.log("port:", info.port);
        console.log("family:", info.family);
        console.log("address:", info.address);

        assert(typeof info.port === "number" && info.port !== null &&
               typeof info.family === "string" && info.family !== null &&
               typeof info.address === "string" && info.address !== null,
               "SocketObject: all socket address information are defined");

        console.log("socket information:");
        console.log("    socket.bufferSize:", sock.bufferSize);
        console.log("    socket.bytesRead:", sock.bytesRead);
        console.log("    socket.bytesWritten:", sock.bytesWritten);
        console.log("    socket.localPort:", sock.localPort);
        console.log("    socket.localAddress:", sock.localAddress);
        console.log("    socket.remotePort:", sock.remotePort);
        console.log("    socket.remoteAddress:", sock.remoteAddress);
        console.log("    socket.remoteFamily:", sock.remoteFamily);

        var properties = [
            [sock.bufferSize, "number", "bufferSize", 1],
            [sock.bytesRead, "number", "bytesRead", 1],
            [sock.bytesWritten, "number", "bytesWritten", 1],
            [sock.localPort, "number", "localPort", 1],
            [sock.localAddress, "string", "localAddress", "love"],
            [sock.remotePort, "number", "remotePort", 1],
            [sock.remoteAddress, "string", "remoteAddress", "love"],
            [sock.remoteFamily, "string", "remoteFamily", "love"]
        ];
        for (var i = 0; i < properties.length; i++) {
            assert(properties[i][2] in sock,
                   "SocketPro: " + properties[i][2] + " is a socket property");
            assert(typeof properties[i][0] === properties[i][1] &&
                   properties[i][0] !== null,
                   "SocketPro: " + properties[i][2] +
                   " is defined as '" + properties[i][0] + "'");

            var propertiesTemp = properties[i][0];
            properties[i][0] = propertiesTemp + properties[i][3];
            assert(properties[i][0] === propertiesTemp,
                   "SocketPro: " + properties[i][2] +
                   " property is read-only");
        }

        var socketonDataFlag = true;
        sock.on("data", function(buf) {
            if (socketonDataFlag) {
                assert(true, "SocketEvent: be called as 'data' event");
                assert(buf.toString("ascii") === "hello",
                       "SocketObject: receive data from client as '" +
                       buf.toString("ascii") + "'");

                socketonDataFlag = false;
            }

            console.log("receive data: " + buf.toString("ascii"));

            if (buf.toString("ascii") === "testSocket") {
                assert(true, "SocketObject: send data without callback function");
                assert(true, "SocketObject: pause and resume receiving data");
            }
        });

        sock.on("error", function(err) {
            console.log("socket connection error: " + err.name);
        });

        var socketonCloseFlag = true;
        sock.on("close", function() {
            if (socketonCloseFlag) {
                assert(true, "SocketEvent: be called as 'close' event");

                assert.result();

                socketonCloseFlag = false;
            }

            console.log("socket connection is closed");
        });

        var socketonTimeoutFlag = true;
        sock.on("timeout", function() {
            if (socketonTimeoutFlag) {
                assert(true, "SocketEvent: be called as 'timeout' event");

                socketonTimeoutFlag = false;
            }

            console.log("socket connection is timeout");
        });

        var Count = 0;
        var Timer = setInterval(function() {
            if (Count === 1) {
                sock.write(new Buffer("hello\r\n"), function() {
                    assert(true,
                           "SocketObject: send data with callback function");

                    console.log("send data: 'hello'");
                });
            }

            if (Count === 2) {
                sock.pause();
                sock.write(new Buffer("test\r\n"));

                console.log("send data: 'test'");
            }

            if (Count === 3) {
                sock.resume();
                sock.write(new Buffer("Socket\r\n"));

                console.log("send data: 'Socket'");
            }

            if (Count === 4) {
                sock.setTimeout(5000);
            }

            if (Count === 7) {
                sock.write(new Buffer("close\r\n"));

                console.log("send data: 'close'");

                clearInterval(Timer);
            }

            Count = Count + 1;
        }, 3000);
    });

    assert(typeof server === "object" && server !== null,
          "ServerObject: server is defined with callback function");

    var serverOptions = {
        port: IPv6Port,
        host: IPv6Address,
        family: IPv6Family
    }
    server.listen(serverOptions, function() {
        assert(true, "ServerObject: begin to listen with callback function");
    });

    server.on("close", function() {
        console.log("TCP server and all connections are closed");
    });

    var connectionFlag = true;
    server.on("connection", function(sock) {
        if (connectionFlag) {
            assert(true, "ServerEvent: be called as 'connection' event");

            server.getConnections(function(error, count) {
                assert(typeof count === "number" && count === 1,
                       "ServerObject: get socket connection as '1'");
            });

            connectionFlag = false;
        }

        server.getConnections(function(error, count) {
            console.log("There is " + count + " socket connections");
        });

        console.log("TCP server accept connection: " + sock.address().address +
                    ":" + sock.address().port);
    });

    server.on("listening", function() {
        assert(true, "ServerEvent: be called as 'listening' event");

        var Info = server.address();
        assert(typeof Info === "object" && Info !== null,
               "ServerObject: get server address information");
        assert(typeof Info.port === "number" && Info.port === IPv6Port,
               "ServerObject: get server port as '" + Info.port + "'");
        assert(typeof Info.address === "string" && Info.address === IPv6Address,
               "ServerObject: get server address as '" + Info.address + "'");
        assert(typeof Info.family === "string" && Info.family === "IPv6",
               "ServerObject: get server family as '" + Info.family + "'");

        console.log("server address information:");
        console.log("    server.address:", Info.address);
        console.log("    server.port:", Info.port);
        console.log("    server.family:", Info.family);

        console.log("TCP server listen: " + IPv6Address + ":" + IPv6Port);
        console.log("If on a101 using Bluetooth communication," +
                    " please connect client with Bluetooth:");
        console.log("    Add soft router on linux:");
        console.log("        ip -6 route add 2001:db8::/64 dev bt0");
        console.log("If on K64f:");
        console.log("    Add soft router on linux:");
        console.log("        ip -6 route add 2001:db8::/64 dev eno1");
        console.log("    Add IPv6 address on linux:");
        console.log("        ip -6 addr add 2001:db8::2/64 dev eno1");
    });

    var errorFlag = true;
    server.on("error", function() {
        if (errorFlag) {
            assert(true, "ServerEvent: be called as 'error' event");

            errorFlag = false;
        }

        console.log("create TCP server failed");
    });
}, 3000);
