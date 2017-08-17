// Copyright (c) 2017, Intel Corporation.

console.log("Test SPI APIs");
console.log("Wire IO11 to IO12");

var spi = require("spi");
var assert = require("Assert.js");
var board = require('board');

var spiBus, spiBuffer, bufferData, busNum;
var deviceNum = 1;
var busSpeed = 20000;
busNum = board.name === "arduino_101" ? 1 : 0;

assert.throws(function() {
    spiBus = spi.open({speed:busSpeed, bus:128, polarity:0, phase:0, bits:16});
}, "SPIBusObject: set SPI bus as '128'");

spiBus = spi.open({speed:busSpeed, bus:busNum, polarity:0, phase:0, bits:16});
assert(typeof spiBus === "object" && spiBus !== null,
       "SPIBusObject: be defined");

spiBuffer = "testNumber";
bufferData = [04, 05, 06, 07];
spiBuffer = spiBus.transceive(deviceNum, bufferData);
assert(spiBuffer.toString("hex") !== "testNumber" &&
       spiBuffer.toString("hex") === "04050607",
       "SPIBusObject: read and write buffer as 'number'");

spiBuffer = "testString";
bufferData = "Hello World";
spiBuffer = spiBus.transceive(deviceNum, bufferData);
assert(spiBuffer.toString("ascii") !== "testString" &&
       spiBuffer.toString("ascii") === bufferData,
       "SPIBusObject: read and write buffer as 'string'");

spiBus.close();

spiBuffer = "testClose";
bufferData = "closeTest";
spiBuffer = spiBus.transceive(deviceNum, bufferData);
assert(spiBuffer !== "closeTest" && spiBuffer === null,
       "SPIBusObject: SPI bus is closed");

var spiBusBits = [4, 8, 16];
var dataString = "testSPIBusTransceive";
var dataNumber = [
    [1, 2],
    [1, 2, 3],
    [1, 2, 3, 4],
    [1, 2, 3, 4, 5, 6, 7, 8],
    [80, 90]
];

var checkNumber = [
    ["0102", "010203", "01020304", "0102030405060708", "000a"],
    ["0102", "010203", "01020304", "0102030405060708", "505a"],
    ["0102", "010200", "01020304", "0102030405060708", "505a"]
];

for (var i = 0; i < spiBusBits.length; i++) {
    var init = {
        speed: busSpeed,
        bus: busNum,
        polarity: 0,
        phase: 0,
        bits: spiBusBits[i]
    }

    spiBus = spi.open(init);

    for (var j = 0; j < dataNumber.length; j++) {
        if (spiBusBits[i] === 16 && j === 1) {
            continue;
        } else {
            var bufferNumber = spiBus.transceive(deviceNum, dataNumber[j]);
            assert(bufferNumber.toString("hex") === checkNumber[i][j],
                   "SPIBusTransceive: read and write number array by bits '" +
                   spiBusBits[i] + "'");
        }
    }

    if (0 < i) {
        var bufferString = spiBus.transceive(deviceNum, dataString);
        assert(bufferString.toString("ascii") === "testSPIBusTransceive",
               "SPIBusTransceive: read and write string by bits '" +
               spiBusBits[i] + "'");
    }

    spiBus.close();
}

assert.result();
