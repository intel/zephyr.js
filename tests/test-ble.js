// Copyright (c) 2016-2017, Intel Corporation.

console.log("Test BLE APIs as server");
console.log("Wire IO7 to IO8");

var gpio = require("gpio");
var ble = require ("ble");
var assert = require("Assert.js");

var deviceName, bufferData, pinA, pinB,
    disconnectClient, acceptclient, readValue;

var clientCount = 0;
var clients = {
    client1: null,
    client2: null,
    client3: null,
    client4: null
}

var acceptMoreClientsFlag = true;
var maxClientsFlag = true;
var acceptClients = function(clientAddress) {
    if (clients.client1 === null) {
        clients.client1 = clientAddress;
        clientCount++;
    } else if (clients.client2 === null) {
        clients.client2 = clientAddress;
        clientCount++;

        if (acceptMoreClientsFlag) {
            assert(1 < clientCount && clientCount < 5,
                   "connection: accept multiple clients to connected");

            acceptMoreClientsFlag = false;
        }
    } else if (clients.client3 === null) {
        clients.client3 = clientAddress;
        clientCount++;
        return clientCount;
    } else if (clients.client4 === null) {
        clients.client4 = clientAddress;
        clientCount++;

        if (maxClientsFlag) {
            assert(clientCount === 4,
                   "connection: accept max clients to connected");

            maxClientsFlag = false;
        }
    } else {
        console.log("Can not connect more client");
    }

    return clientCount;
}

var disconnectMoreClientsFlag = false;
var disconnectClients = function(clientAddress) {
    if (clients.client4 === clientAddress) {
        clients.client4 = null;
        clientCount--;
    } else if (clients.client3 === clientAddress) {
        clients.client3 = null;
        clientCount--;
    } else if (clients.client2 === clientAddress) {
        clients.client2 = null;
        clientCount--;
    } else if (clients.client1 === clientAddress) {
        clients.client1 = null;
        clientCount--;
    } else {
        console.log("Can not disconnect client");
    }

    return clientCount;
}

var writeValue = 1;
pinA = gpio.open({pin: "IO7"});
pinB = gpio.open({pin: "IO8", mode: "in", edge: "none"});
pinA.write(writeValue);
readValue = pinB.read();

bufferData = new Buffer(1);
bufferData.writeUInt8(0);

var writeCharacteristic = new ble.Characteristic({
    uuid: "aa00",
    properties: ["write"],
    descriptors: [
        new ble.Descriptor({
            uuid: "2901",
            value: "write"
        })
    ]
});

var writeRequestFlags = {
    offset: true,
    type: true,
    length: true,
    success: true
}

var writeFlag = true;
writeCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    if (writeFlag) {
        assert(pinB.read() === writeValue,
               "characteristic: respond to write request");

        writeFlag = false;
    }

    if (data.toString("hex") === "01") {
        writeValue = 1;
    } else if (data.toString("hex") === "00") {
        writeValue = 0;
    } else {
        writeValue = null;
    }

    bufferData = data;
    if (offset !== 0) {
        if (writeRequestFlags.offset) {
            assert(writeRequestFlags.offset,
                   "result: RESULT_INVALID_OFFSET");

            writeRequestFlags.offset = false;
        }

        callback(this.RESULT_INVALID_OFFSET);
        return;
    } else if (bufferData.length !== 1) {
        if (writeRequestFlags.length) {
            assert(writeRequestFlags.length,
                   "result: RESULT_INVALID_ATTRIBUTE_LENGTH");

            writeRequestFlags.length = false;
        }

        console.log("Please write value as '0x00' by writeCharacteristic");

        callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
        return;
    } else if (typeof writeValue !== "number") {
        if (writeRequestFlags.type) {
            assert(writeRequestFlags.type, "result: RESULT_UNLIKELY_ERROR");

            writeRequestFlags.type = false;
       }

        console.log("Please write value as '0x5555' by writeCharacteristic");

        callback(this.RESULT_UNLIKELY_ERROR);
        return;
    } else {
        if (writeRequestFlags.success) {
            assert(writeRequestFlags.success, "result: RESULT_SUCCESS");

            writeRequestFlags.success = false;
        }

        pinA.write(writeValue);
        console.log("Please read value by readCharacteristic");

        callback(this.RESULT_SUCCESS);
    }
};

var readCharacteristic = new ble.Characteristic({
    uuid: "aa01",
    properties: ["read", "notify"],
    descriptors: [
        new ble.Descriptor({
            uuid: "2901",
            value: "read"
        })
    ]
});

var readFlag = true;
readCharacteristic.onReadRequest = function(offset, callback) {
    if (readFlag) {
        assert(pinB.read() === writeValue,
               "characteristic: respond to read request");

        console.log("Please subscribe by readCharacteristic");

        readFlag = false;
    }

    readValue = pinB.read();
    callback(this.RESULT_SUCCESS, bufferData);
};

var notifyFlag = true;
readCharacteristic.onSubscribe = function(maxValueSize, callback) {
    if (notifyFlag) {
        assert(readValue === writeValue,
               "characteristic: respond to subscribe request");

        notifyFlag = false;
    }

    readValue = pinB.read();

    setTimeout(function() {
        ble.disconnect(acceptclient);
    }, 3000);

    callback(bufferData);
};

var poweredOnFlag = false;
var setServicesFlag = false;
ble.on("stateChange", function(state) {
    if (state === "poweredOn") {
        poweredOnFlag = true;
        deviceName = "BLE SERVICE is very good and so cool";

        assert.throws(function () {
            ble.startAdvertising(deviceName, ["abcdef"]);
        }, "BLEService: advertising with invalid deviceName and UUID");

        deviceName = "BLE SERVICE";
        ble.startAdvertising(deviceName, ["abcd"]);

        ble.updateRssi();

        ble.setServices([
            new ble.PrimaryService({
                uuid: "a000",
                characteristics: [
                    writeCharacteristic,
                    readCharacteristic
                ]
            })
        ], function(error) {
            console.log("service: error " + error.name);
            return;
        });

        setServicesFlag = true;
    }
});

var advertiseFlag = false;
ble.on("advertisingStart", function(error) {
    advertiseFlag = true;
});

var acceptFirstFlag = true;
ble.on("accept", function(clientAddress) {
    var count = acceptClients(clientAddress);

    console.log("Accept " + clientAddress);
    console.log("There are " + count + " clients to connected");

    if (acceptFirstFlag) {
        assert(clientAddress !== null,
               "connection: accept to connected");

        acceptclient = clientAddress;

        console.log("Please write value as '0x05' by writeCharacteristic");

        acceptFirstFlag = false;
    }
});

var disconnectFirstFlag = true;
ble.on("disconnect", function(clientAddress) {
    var count = disconnectClients(clientAddress);

    console.log("Disconnect " + clientAddress);
    console.log("There are " + count + " clients to connected");

    if (clients.client1 === null &&
        clients.client2 === null &&
        clients.client3 === null &&
        clients.client4 === null) {
        if (disconnectMoreClientsFlag) {
            assert(clientCount === 0,
                   "connection: disconnect all clients");

            assert.result();

            disconnectMoreClientsFlag = false;
        }
    }

    if (disconnectFirstFlag) {
        assert(clientAddress !== null,
               "connection: " + clientAddress + " is disconnected");

        disconnectClient = clientAddress;
        assert(disconnectClient === acceptclient,
               "connection: " + clientAddress + " connected and disconnected");

        console.log("Please connected multiple clients and max clients as '4'");

        disconnectMoreClientsFlag = true;
        disconnectFirstFlag = false;
    }
});

var rssiFlag = false;
var rssiValueFlag = false;
ble.on("rssiUpdate", function(rssi) {
    if (rssi === -50) rssiValueFlag = true;

    rssiFlag = true;
});

setTimeout(function() {
    assert(poweredOnFlag, "BLEService: powered on");
    assert(setServicesFlag, "BLEService: be creating");
    assert(advertiseFlag, "BLEService: advertising is starting");
    assert(rssiValueFlag && rssiFlag, "BLEService: set default RSSI value");

    console.log("Please connect one BLE client");
}, 3000);
