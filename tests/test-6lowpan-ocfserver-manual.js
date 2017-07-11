// Copyright (c) 2017, Intel Corporation.

// Build this test case and check random ble address,
//   and check default device name as "ZJS Device":
//     make JS=tests/test-6lowpan-ocfserver-manual.js ROM=256
// Build this test case with BLE_ADDR flag again:
//     make JS=tests/test-6lowpan-ocfserver-manual.js ROM=256 BLE_ADDR="F2:E3:D4:C5:B6:A7"
// Build this test case with DEVICE_NAME flag again:
//     make JS=tests/test-6lowpan-ocfserver-manual.js ROM=256 DEVICE_NAME="MyServer"

console.log("Test 6LoWPAN APIs with ocf server");

var ocf = require("ocf");
var server = ocf.server;

console.log("Please connect server with Bluetooth....\n");
console.log("If build with 'BLE_ADDR' flag:");
console.log("    OCF BLE address: expected result 'F2:E3:D4:C5:B6:A7'");
console.log("If not:");
console.log("    OCF BLE address: expected result 'random'\n");

console.log("If build with 'DEVICE_NAME' flag:");
console.log("    OCF BLE address: expected result 'MyServer'");
console.log("If not:");
console.log("    OCF BLE address: expected result 'ZJS Device'");

var TestProperties = {
    state: true
}

var resourceInit = {
    resourcePath: "/test/OCFserver",
    resourceTypes: ["core.test"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: TestProperties
}

server.on("retrieve", function onRetrieve(request, observe) {});
server.on("update", function onUpdate(request) {});

server.register(resourceInit).then(function(resource) {
    console.log("Registered OCF server");
}).catch(function(error) {
    console.log("OCFServer error: " + error.name + ":" + error.message);
});

ocf.start();
