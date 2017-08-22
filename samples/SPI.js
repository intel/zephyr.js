// Copyright (c) 2017, Intel Corporation.

// Sample code for showing how to to use SPI on ZJS

console.log("SPI test starting..");
try {
    var spi = require("spi");
    var board = require('board');
    // Arduino 101 uses SPI_1 while others use SPI_0
    var busNum = board.name === "arduino_101" ? 1 : 0;
    // The deviceNumber is the slave device we want to send data to
    var deviceNum = 1;

    console.log("BOARD NAME = " + board.name + " Device Number = " + deviceNum);

    var spiBus = spi.open({bus:busNum, polarity:0, phase:0, bits:16});
    var buffer = spiBus.transceive(deviceNum, "Hello World\0");
    console.log("From SPI device " + deviceNum + ": " + buffer.toString('hex'));

    buffer = spiBus.transceive(deviceNum, [1, 2, 3, 4]);
    console.log("From SPI device " + deviceNum + ": " + buffer.toString('hex'));

    var bufobj = new Buffer("Hello World\0");
    buffer = spiBus.transceive(deviceNum, bufobj);
    console.log("From SPI device " + deviceNum + ": " + buffer.toString('hex'));

    buffer = spiBus.transceive(deviceNum, new Buffer([1, 2, 3, 4]));
    console.log("From SPI device " + deviceNum + ": " + buffer.toString('hex'));

    spiBus.close();
    console.log("Closed");

    // This should fail
    buffer = spiBus.transceive(deviceNum, [1, 2, 3, 4]);

    // Open again
    spiBus = spi.open({bus:busNum, polarity:0, phase:0, bits:8});
    buffer = spiBus.transceive(deviceNum, [1, 2, 3, 4]);
    console.log("From SPI device " + deviceNum + ": " + buffer.toString('hex'));
} catch (err) {
  console.log("SPI error: " + err.message);
}
