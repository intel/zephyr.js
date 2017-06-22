// Copyright (c) 2017, Intel Corporation.

console.log("Test SPI APIs");
console.log("Wire IO11 to IO12");

var spi = require("spi");
var assert = require("Assert.js");

var spiBus, spiBuffer, bufferData;
assert.throws(function() {
    spiBus = spi.open({speed:20, bus:128, polarity:0, phase:0, bits:16});
}, "SPIBusObject: set SPI bus as '128'");

spiBus = spi.open({speed:20, bus:1, polarity:0, phase:0, bits:16});
assert(typeof spiBus === "object" && spiBus !== null,
       "SPIBusObject: be defined");

spiBuffer = "testNumber";
bufferData = [04, 05, 06, 07];
spiBuffer = spiBus.transceive(1, bufferData);
assert(spiBuffer.toString("hex") !== "testNumber" &&
       spiBuffer.toString("hex") === "04050607",
       "SPIBusObject: read and write buffer as 'number'");

spiBuffer = "testString";
bufferData = "Hello World";
spiBuffer = spiBus.transceive(1, bufferData);
assert(spiBuffer.toString("ascii") !== "testString" &&
       spiBuffer.toString("ascii") === bufferData,
       "SPIBusObject: read and write buffer as 'string'");

spiBus.close();

spiBuffer = "testClose";
bufferData = "closeTest";
spiBuffer = spiBus.transceive(1, bufferData);
assert(spiBuffer !== "closeTest" && spiBuffer === null,
       "SPIBusObject: SPI bus is closed");

var spiBusBits = [4, 8, 16];
var dataString = "testSPIBusTransceive";
var dataNumber = [
    [1, 2],
    [1, 2, 3],
    [1, 2, 3, 4, 5, 6, 7, 8],
    [80, 90]
];

var checkNumber = [
    ["0102", "010203", "0102030405060708", "000a"],
    ["0102", "010203", "0102030405060708", "505a"],
    ["0102", "010200", "0102030405060708", "505a"]
];

for (var i = 0; i < spiBusBits.length; i++) {
    var init = {
        speed: 20,
        bus: 1,
        polarity: 0,
        phase: 0,
        bits: spiBusBits[i]
    }

    spiBus = spi.open(init);

    for (var j = 0; j < dataNumber.length; j++) {
        var bufferNumber = spiBus.transceive(1, dataNumber[j]);
        assert(bufferNumber.toString("hex") === checkNumber[i][j],
               "SPIBusTransceive: read and write number array by bits '" +
               spiBusBits[i] + "'");
    }

    if (0 < i) {
        var bufferString = spiBus.transceive(1, dataString);
        assert(bufferString.toString("ascii") === "testSPIBusTransceive",
               "SPIBusTransceive: read and write string by bits '" +
               spiBusBits[i] + "'");
    }

    spiBus.close();
}

assert.result();
