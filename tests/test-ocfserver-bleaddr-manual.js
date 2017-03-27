// Copyright (c) 2017, Intel Corporation.

console.log("Test setBleAddress()");

var ocf = require("ocf");

var server = ocf.server;

var BleAddress = "F1:E2:D3:C4:B5:A6";
ocf.setBleAddress(BleAddress);
console.log("Please connect server with Bluetooth");
console.log("OCF BLE address: expected result " + BleAddress);

var TestProperties = {
    state: true
}

var resourceInit = {
    resourcePath: "/test/OCFserver",
    resourceTypes: ["core.test"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestProperties
}

server.on("retrieve", function onRetrieve(request, observe) {});
server.on("update", function onUpdate(request) {});

server.register(resourceInit).then(function(resource) {
    server.notify(resource);
}).catch(function(error) {
    console.log("OCFServer error: " + error.name);
});
