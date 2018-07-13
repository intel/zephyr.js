ZJS API for SPI
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [SPI API](#spi-api)
  * [spi.open(options)](#spiopenoptions)
* [SPIBus API](#spibus-api)
  * [spiBus.transceive(target, data, direction)](#spibustransceivetarget-data-direction)
  * [spiBus.close()](#spibusclose)

Introduction
------------
The SPI API supports the Serial Peripheral Interface, a synchronous
serial protocol that allows multiple slave chips to communicate with a
master chip.  A single SPI bus uses the following pins: SCLK for
clock, MOSI (Master Out, Slave In) for write, MISO (Master In, Slave
Out) for read, and one or more SS (Slave Select) for selecting the
slave device.

For each clock signal, one bit is written from the master to the
selected slave, and one bit is read by the master from the selected
slave, so there is one transceive operation, instead of a separate
read and write.

When a slave device's chip select is 0 (low), it communicates with the
master; otherwise it ignores the master. The master can select
multiple slaves in a write-only configuration; in this case, no slave
is writing data, each only reads.

Since the SS pins may be connected to slave chip select through a
demultiplexer and thereby work as an address bus, slave devices are
identified by an index in this API, rather than by SS pins. Also,
since multiple SPI buses may be present on a board, these are
identified by an index in this API. Implementations SHOULD encapsulate
the mapping from SPI bus number and device number to the physical SPI
pins.

Note that on the Arduino 101, using SPI will cause one of the onboard LEDs to
become unavailable.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>// require returns a SPI object
// var spi = require('spi');
[ReturnFromRequire]
interface SPI {
    SPIBus open(SPIOptions init);
};<p>
dictionary SPIOptions {
    octet bus;
    long speed;  // bus clock frequency in Hz
    boolean msbFirst;
    long polarity;
    long phase;
    unsigned long frameGap;
    string topology;
};<p>[ExternalInterface=(Buffer)]
interface SPIBus {
    void transceive(octet target, Buffer data, string direction);
    void close();
};
</pre>
</details>

SPI API
-------
### spi.open(options)
* `options` *SPIOptions* The `options` object lets you pass optional values to use instead of the defaults.
* Returns: an SPIBus object.

Note these `options` values can't be changed once the SPI object is
created.  If you need to change the settings afterwards, you'll need
to use the 'close' command and create a new SPI object with the
settings you desire.

SPIBus API
----------
### spiBus.transceive(target, data, direction)
* `target` *octet* The number identifying the slave.
* `data` *Buffer* The data to be written to, and returned from, the slave.
* `direction` *string*

Writes data buffer using SPI to the slave identified by the target argument, and
reads from the slave device into a readBuffer that is returned.  The read and
write buffers are the same size.

### spiBus.close()

Closes the SPI connection.

Sample Apps
-----------
* [SPI sample](../samples/SPI.js)
