// Copyright (c) 2017-2018, Intel Corporation.

console.log("Test WebUSB APIs");

var webusb = require("webusb");

console.log("Please run Google Chrome >= 60");

var URL = "https://intel.github.io/zephyr.js/webusb";
webusb.setURL(URL);
console.log("\nsetURL(URL): " + URL);
console.log("expected result: a notification prompting");

webusb.on("read", function(data) {
    console.log("\nReceived: " + data.toString("hex"));
});

var count = 1;
var longData =
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "12345678901234567890123456789012345678901234567890" +
    "123456789012";  // length 512

var webusbTimer = setInterval(function () {
    if (count === 1) {
        webusb.write(new Buffer("ZJS is great!"));
        console.log("\nwrite(buffer): 'ZJS is great!'");
        console.log("expected result: sent packet with 13 bytes");
    }

    if (count === 2) {
        console.log("\nplease click 'Send/Receive' button");
        console.log("on('read', func):");
        console.log("expected result: received packet with 32 bytes");
    }

    if (count === 3) {
        URL = "www.github.com";
        webusb.setURL(URL);
        console.log("\nsetURL(URL): " + URL);
        console.log("expected result: no notification prompting");
    }

    if (count === 4) {
        try {
            webusb.write(new Buffer(longData));
        } catch (e) {
            console.log("\n" + e.name + " : " + e.message);
            console.log("write(buffer): 512 bytes data");
            console.log("expected result: throwout an error");
        }
    }

    if (count === 5) {
        console.log("\nTesting completed");

        clearInterval(webusbTimer);
    }

    count++;
}, 20000);
