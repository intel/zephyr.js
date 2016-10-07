ZJS API for I2C
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The I2C API supports the I2C protocol, which allows multiple slave chips to
communicate with one or more master chips.  Each I2C bus has two signals - SDA
and SCL. SDA is the data signal and SCL is the clock signal.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a I2C object
// var i2c = require('i2c');

[NoInterfaceObject]
interface I2C {
    I2CBus open(I2CInit init);
};

dictionary I2CInit {
    octet bus;
    I2CBusSpeed speed;
};

[NoInterfaceObject]
interface I2CBus {
    // has all the properties of I2CInit as read-only attributes
    write(octet device, Buffer data);
};
```

API Documentation
-----------------
### I2C.open

`I2CBus open(I2CInit init);`

The `init` object lets you set the I2C bus you wish to use and the speed you
want to operate at.  Speed options are 10, 100, 400, 1000, and 34000. Speed is
measured in kbs.

### I2CBus.write

`void write(octet device, Buffer data);`

Writes the data to the given device address. The first byte of data typically
contains the register you want to write the data to.  This will vary from device
to device.

### I2CBus.read

`void read(octet device, unsigned int size, octet registerAddress);`

Reads 'size' bytes of data from the device at the registerAddress. The default
value of registerAdress is 0x00;

### I2CBus.burstRead

`void burstRead(octet device, unsigned int size, octet registerAddress);`

Reads 'size' bytes of data from the device across multiple addresses starting
at the registerAddress. The default value of registerAdress is 0x00;

Sample Apps
-----------
* [I2C sample](../samples/I2C.js)
