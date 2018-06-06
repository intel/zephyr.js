ZJS API for I2C
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [I2C API](#i2c-api)
  * [i2c.open(init)](#i2copeninit)
* [I2CBus API](#i2cbus-api)
  * [i2cBus.write(device, data)](#i2cbuswritedevice-data)
  * [i2cBus.read(device, size, registerAddress)](#i2cbusreaddevice-size-registeraddress)
  * [I2CBus.burstRead(device, size, registerAddress)](#i2cbusburstreaddevice-size-registeraddress)
* [Sample Apps](#sample-apps)

Introduction
------------
The I2C API supports the I2C protocol, which allows multiple slave chips to
communicate with one or more master chips.  Each I2C bus has two signals - SDA
and SCL. SDA is the data signal and SCL is the clock signal.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>// require returns a I2C object
// var i2c = require('i2c');<p><p>[ReturnFromRequire]
interface I2C {
    I2CBus open(I2CInit init);
};<p>dictionary I2CInit {
    octet bus;
    I2CBusSpeed speed;
};<p>[ExternalInterface=(buffer,Buffer)]
interface I2CBus {
    // has all the properties of I2CInit as read-only attributes
    void write(octet device, Buffer data);
    void read(octet device, unsigned long size, octet registerAddress);
    void burstRead(octet device, unsigned long size, octet registerAddress);
};</pre>
</details>

I2C API
-------
### i2c.open(init)
* `init` *I2CInit* Lets you set the I2C bus you wish to use and the speed you
want to operate at. Speed options are 10, 100, 400, 1000, and 34000. Speed is
measured in kbs.
* Returns: an I2CBus object.

I2CBus API
----------
### i2cBus.write(device, data)
* `device` *octet* The device address.
* `data` *Buffer* The data to be written.

Writes the data to the given device address. The first byte of data typically
contains the register you want to write the data to.  This will vary from device
to device.

### i2cBus.read(device, size, registerAddress)
* `device` *octet* The device address.
* `size` *unsigned long* The number of bytes of data to read.
* `registerAddress` *octet* The register on the device from which to read.

Reads 'size' bytes of data from the device at the registerAddress. The default
value of registerAdress is 0x00;

### I2CBus.burstRead(device, size, registerAddress)
* `device` *octet* The device address.
* `size` *long* The number of bytes of data to read.
* `registerAddress` *octet* The number of the starting address from which to read.

Reads 'size' bytes of data from the device across multiple addresses starting
at the registerAddress. The default value of registerAdress is 0x00;

Sample Apps
-----------
* [I2C sample](../samples/I2C.js)
* [BMP280 temp](../samples/I2CBMP280.js)
