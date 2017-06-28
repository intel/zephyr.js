// Copyright (c) 2017, Intel Corporation.

// Sample code for showing how to to use SPI on ZJS

console.log("SPI test starting..");
try {
    var spi = require("spi");
    var spiBus1 = spi.open({speed:20000, bus:1, polarity:0, phase:0, bits:16});

    var buffer = spiBus1.transceive(1, "Hello World\0");
    console.log("From SPI device 1: " + buffer.toString('hex'));

    buffer = spiBus1.transceive(1, [1, 2, 3, 4]);
    console.log("From SPI device 1: " + buffer.toString('hex'));

    var bufobj = new Buffer("Hello World\0");
    buffer = spiBus1.transceive(1, bufobj);
    console.log("From SPI device 1: " + buffer.toString('hex'));

    buffer = spiBus1.transceive(1, new Buffer([1, 2, 3, 4]));
    console.log("From SPI device 1: " + buffer.toString('hex'));

    spiBus1.close();
    console.log("Closed");

    // This should fail
    buffer = spiBus1.transceive(1, [1, 2, 3, 4]);

    // Open again
    spiBus1 = spi.open({speed:20000, bus:1, polarity:0, phase:0, bits:8});
    buffer = spiBus1.transceive(1, [1, 2, 3, 4]);
    console.log("From SPI device 1: " + buffer.toString('hex'));
} catch (err) {
  console.log("SPI error: " + err.message);
}
