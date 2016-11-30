// Copyright (c) 2016, Intel Corporation.

console.log("Wire IO2 to IO4");

var gpio = require("gpio");
var pins = require("arduino101_pins");
var ble = require ("ble");

var total = 0;
var passed = 0;
function assert(actual, description) {
    total += 1;
    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }
    console.log(label + " - " + description);
}

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (e) {
        threw = true;
    }
    assert(threw, description);
}

var deviceName, bufferData, pinA, pinB, disconnectClient,
    readValue, acceptClient, rssiValue, tmpValue;
var writeValue = true;
var poweredOnFlag = false;
var advertisingFlag = false;
var writeFlag = true;
var readFlag = true;
var rssiFlag = true;
var notifyFlag = true;
var acceptFlag = true;
var disconnectFlag = false;
var advertiseFlag = true;
var totalFlag = false;
var stopFlag = true;

pinA = gpio.open({ pin: pins.IO2 });
pinB = gpio.open({ pin: pins.IO4, direction: "in" });
pinA.write(writeValue);
readValue = pinB.read();

bufferData = new Buffer(1);
bufferData.writeUInt8(0);

var writeCharacteristic = new ble.Characteristic({
    uuid: 'aa00',
    properties: ['write'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'write'
        })
    ]
});

var keywordResult = [ true, true, true, true ];

writeCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    if (data.toString('hex') === "01") {
        writeValue = true;
    } else if (data.toString('hex') === "00") {
        writeValue = false;
    } else {
        tmpValue = writeValue;
        writeValue = null;
    }

    if (offset !== 0) {
        if (keywordResult[3]) {
            keywordResult[3] = false;
            assert(true, "result: RESULT_INVALID_OFFSET");
        }

        callback(this.RESULT_INVALID_OFFSET);
        return;
    }

    if (typeof writeValue !== "boolean") {
        if (keywordResult[1]) {
            keywordResult[1] = false;
            assert(true, "result: RESULT_UNLIKELY_ERROR");
        }

        writeValue = tmpValue;
        callback(this.RESULT_UNLIKELY_ERROR);
        return;
    }

    bufferData = data;
    if (bufferData.length !== 1) {
        if (keywordResult[2]) {
            keywordResult[2] = false;
            assert(true, "result: RESULT_INVALID_ATTRIBUTE_LENGTH");
        }

        callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
        return;
    }

    pinA.write(writeValue);
    if (keywordResult[0]) {
        keywordResult[0] = false;
        assert(true, "result: RESULT_SUCCESS");
    }

    callback(this.RESULT_SUCCESS);

    if (writeFlag) {
        assert(pinB.read() === writeValue,
               "characteristic: respond to write request");

        writeFlag = false;
    }
};

var readCharacteristic = new ble.Characteristic({
    uuid: 'aa01',
    properties: ['read', 'notify'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'read'
        })
    ]
});

readCharacteristic.onReadRequest = function(offset, callback) {
    readValue = pinB.read();
    callback(this.RESULT_SUCCESS, bufferData);

    if (readFlag) {
        assert(pinB.read() === writeValue,
               "characteristic: respond to read request");

        readFlag = false;
    }
};

readCharacteristic.onSubscribe = function(maxValueSize, callback) {
    readValue = pinB.read();
    callback(bufferData);

    if (notifyFlag) {
        assert(readValue === writeValue,
               "characteristic: respond to subscribe request");

        notifyFlag = false;
    }
};

ble.on('stateChange', function (state) {
    if (state === 'poweredOn') {
        assert(true, "start-up: powered on");
        poweredOnFlag = true;
        deviceName = "BLE SERVICE is very good and so cool";

        expectThrow("advertising: invalid deviceName and UUID", function () {
            ble.startAdvertising(deviceName, ['abcdef']);
        });

        deviceName = "BLE SERVICE";
        ble.startAdvertising(deviceName, ['abcd']);

        ble.updateRssi();

        ble.setServices([
            new ble.PrimaryService({
                uuid: 'a000',
                characteristics: [
                    writeCharacteristic,
                    readCharacteristic
                ]
            })
        ], function (error) {
            if (error) {
                assert(false, "service: fail to setup");
            }
        });
    }
});

ble.on('advertisingStart', function (error) {
    if (error) {
        assert(false, "advertising: be starting");
        return;
    }

    if (advertiseFlag) {
        assert(true, "advertising: starting with deviceName and UUID");

        advertiseFlag = false;
    }
});

ble.on('accept', function (clientAddress) {
    if (acceptFlag) {
        assert(clientAddress !== null,
               "connection: Accept " + clientAddress + " to connect");

        acceptFlag = false;
    }

    acceptclient = clientAddress;

    if (stopFlag) {
        setTimeout(function () {
            ble.disconnect();
            console.log("please connect again");
        }, 1000);
    }
});

ble.on('disconnect', function (clientAddress) {
    if (disconnectFlag) {
        assert(clientAddress !== null,
               "connection: " + clientAddress + " is disconnected");

        disconnectFlag = false;
        totalFlag = true;
    }

    disconnectClient = clientAddress;

    if (stopFlag) {
        assert(disconnectClient === acceptclient,
               "disconnect: " + clientAddress
               + " connected and disconnected");

        disconnectFlag = true;
        stopFlag = false;
    }

    if (totalFlag) {
        totalFlag = false;

        console.log("TOTAL: " + passed + " of " + total + " passed");
    }
});

ble.on('rssiUpdate', function (rssi) {
    if (rssiFlag) {
        assert(rssi === -50, "rssi: set default RSSI value");

        rssiFlag = false;
    }
});

setTimeout(function () {
    if (!poweredOnFlag) {
        assert(false, "start-up: powered on");
    }
}, 1000);
