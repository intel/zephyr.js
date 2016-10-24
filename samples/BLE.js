// Copyright (c) 2016, Intel Corporation.

// Register a BLE echo service that expose read/write capabilities
// write will allow client to send bytes to be stored
// read will allow client to read back the stored value, or 0 if not found

var ble = require ("ble");

var deviceName = 'BLE Test';

var echoValue = new Buffer(1);
echoValue.writeUInt8(0);

ble.on('stateChange', function(state) {
    if (state === 'poweredOn') {
        ble.startAdvertising(deviceName, ['ab00']);
    }
});

ble.on('accept', function(clientAddress) {
    console.log("Accepted Connection: " + clientAddress);
    ble.updateRssi();
});

ble.on('disconnect', function(clientAddress) {
    console.log("Disconnected Connection: " + clientAddress);
});

ble.on('rssiUpdate', function(rssi) {
    console.log("RSSI value: " + rssi + "dBm");
});

ble.on('advertisingStart', function(error) {
    if (error) {
        console.log("Advertising start error: " + error);
        return;
    }

    ble.setServices([
        new ble.PrimaryService({
            uuid: 'ab00',
            characteristics: [
                new ble.Characteristic({
                    uuid: 'ab01',
                    properties: ['read', 'write'],
                    descriptors: [
                        new ble.Descriptor({
                            uuid: '2901',
                            value: 'Echo'
                        })
                    ],
                    onReadRequest: function(offset, callback) {
                        console.log("Read value: " + echoValue.toString('hex'));
                        callback(this.RESULT_SUCCESS, echoValue);
                    },
                    onWriteRequest: function(data, offset, withoutResponse,
                                             callback) {
                        console.log("Write value: " + data.toString('hex'));
                        echoValue = data;
                        callback(this.RESULT_SUCCESS);
                    }
                })
            ]
        })
    ], function(error) {
        if (error) {
            console.log("Set services error: " + error);
        }
    });
});

console.log("BLE sample...");
