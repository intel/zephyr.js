ZJS API for SPI
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The SPI API supports the Serial Peripheral Interface, a synchronous
serial protocol that allows multiple slave chips to communicate with a master chip.
A single SPI bus uses the following pins: SCLK for clock,
MOSI (Master Out, Slave In) for write, MISO (Master In, Slave Out) for read, and
one or more SS (Slave Select) for selecting the slave device.

For each clock signal one bit is written from the master to the selected slave and
one bit is read by the master from the selected slave, so there is no separate
read and write, but one transceive operation.

When a slave device's chip select is 0 (low), then it communicates with the
master, otherwise it ignores the master. The master can select multiple slaves in
a write-only configuration; in this case no slave is writing data, they only read.

Since the SS pins may be connected to slave chip select through a demultiplexer
and thereby work as an address bus, slave devices are identified by an index in
this API, rather than by SS pins. Also, since multiple SPI buses may be present
on a board, these are identified by an index in this API. Implementations SHOULD
encapsulate the mapping from SPI bus number and device number to the physical SPI
pins.

Note that on the Arduino 101, using SPI will cause one of the onboard LED to
become unavailable.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a SPI object
// var spi = require('spi');

[NoInterfaceObject]
interface SPI {
    SPIBus open(SPIOptions init);
};

dictionary SPIOptions {
    octet bus;
    long speed;
    bool msbFirst;
    long polarity;
    long phase;
    unsigned long frameGap;
    string topology;
    
};

[NoInterfaceObject]
interface SPIBus {
    transceive(octet target, Buffer data, string direction);
    close();
};
```

API Documentation
-----------------
### SPI.open

`SPIBus open(SPIOptions options);`

The `options` object lets you pass optional values to use instead of the defaults.
Note these values can't be changed once the SPI object is created.  If you need
to change the settings afterwards, you'll need to use the 'close' command and
create a new SPI object with the settings you desire.

### SPIBus.transceive

`Buffer transceive(octet target, Buffer data, string direction);`

Writes data buffer using SPI to the slave identified by the target argument, and
reads from the slave device into a readBuffer that is returned.  The read and
write buffers are the same size.

### SPIBus.close

`void close();`

Closes the SPI connection.

Sample Apps
-----------
* [SPI sample](../samples/SPI.js)
