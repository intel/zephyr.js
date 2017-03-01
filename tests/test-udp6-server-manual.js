// Copyright (c) 2017, Intel Corporation.

// Testing UDP Server APIs

console.log("test UDP Server APIs for ipv6");

var dgram = require("dgram");

var PORT = 33333;
var HOST = "2001:db8::1";
var i = 0;

var keyWords = [
    ["hello", "hello"],
    ["WORLD", "ORLD"],
    ["^_^! *_*!", "^! *_*!"],
    ["~!@#$%^&*()_+", "#$%^&*()_+"],
    ["1234567890", "567890"]
];

var server = dgram.createSocket("udp6");

server.on("message", function(message, remote) {
    console.log("received - " + remote.address + ":" +
                remote.port + " - " + message.toString("ascii"));

    if (i < keyWords.length) {
        console.log("dgram.on(message, cb): received expected result '"
                    + keyWords[i][0] + "'");

        if (message.toString("ascii") === keyWords[i][0] + "\n" ||
            message.toString("ascii") === keyWords[i][0] + "\r") {
            server.send(message, i, message.length - i,
                        remote.port, remote.address);

            console.log("dgram.on(message, cb): send expected result '"
                        + keyWords[i][1] + "'");

            i = i + 1;

            if (i === keyWords.length) {
                server.close();

                console.log("Testing completed");
            } else {
                console.log("Please send '" + keyWords[i][0] + "' to " +
                            HOST + ":" + PORT);
            }
        }
    }
});

server.on("error", function(err) {
    console.log("error: " + err);
});

server.bind(PORT, HOST);

console.log("Please send '" + keyWords[i][0] + "' to " + HOST + ":" + PORT);
