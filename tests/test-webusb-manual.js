// Copyright (c) 2017-2018, Intel Corporation.

console.log("Test WebUSB APIs");

var webusb = require("webusb");

console.log("Please run Google Chrome >= 60");

var URL = "https://intel.github.io/zephyr.js/webusb";
webusb.setURL(URL);
console.log("\nsetURL(URL): " + URL);
console.log("expected result: a notification prompting");

setTimeout(function () {
    URL = "www.github.com";
    webusb.setURL(URL);
    console.log("\nsetURL(URL): " + URL);
    console.log("expected result: no notification prompting");
}, 20000);

var readFlag = true;
webusb.on("read", function(data) {
    if (readFlag) {
        console.log("\non('read', func):");
        console.log("expected result: '1 packets sent (32 bytes)' on Chrome");

        readFlag = false;
    }

    console.log("\nReceived: " + data.toString("hex"));
});

setTimeout(function () {
    webusb.write(new Buffer("ZJS is great!"));
    console.log("\nwrite(buffer): 'ZJS is great!'");
    console.log("expected result: '1 packets received (13 bytes)' on Chrome");
}, 40000);

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
setTimeout(function () {
    try {
        webusb.write(new Buffer(longData));
    } catch (e) {
        console.log("\n" + e.name + " : " + e.message);
        console.log("\nwrite(buffer): 512 bytes data");
        console.log("expected result: throwout an error");

        console.log("\nTesting completed");
    }
}, 60000);
