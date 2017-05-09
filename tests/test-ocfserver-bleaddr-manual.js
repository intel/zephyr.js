// Copyright (c) 2017, Intel Corporation.

// Build this test case and check ble address as "F1:E2:D3:C4:B5:A6",
//   and check default device name as "ZJS Device".
// Build this test case with BLE_ADDR flag again:
//     make JS=tests/test-ocfserver-bleaddr-manual.js BLE_ADDR="F2:E3:D4:C5:B6:A7"
// Build this test case with DEVICE_NAME flag again:
//     make JS=tests/test-ocfserver-bleaddr-manual.js DEVICE_NAME="MyServer"

console.log("Test BLE with ocf server");

var ocf = require("ocf");

var server = ocf.server;

var BleAddress = "F1:E2:D3:C4:B5:A6";
ocf.setBleAddress(BleAddress);

console.log("Please connect server with Bluetooth....\n");
console.log("If build with 'BLE_ADDR' flag:");
console.log("    OCF BLE address: expected result 'F2:E3:D4:C5:B6:A7'");
console.log("If not:");
console.log("    OCF BLE address: expected result '" + BleAddress + "'\n");

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

ocf.start();
