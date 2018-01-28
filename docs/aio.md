ZJS API for Analog I/O (AIO)
============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: AIO](#aio-api)
  * [aio.open(AIOInit)](#aioopenaioinit)
* [Class: AIOPin](#aiopin-api)
  * [pin.read()](#pinread)
  * [pin.readAsync(ReadCallback)](#pinreadasyncreadcallback)
  * [pin.on(eventType, ReadCallback)](#pinoneventtype-readcallback)
  * [pin.close()](#pinclose)
* [Sample Apps](#sample-apps)

Introduction
------------
The AIO API supports analog I/O pins. So far, the support is just for
analog-to-digital conversion (ADC). This measures an analog voltage input on a
pin and converts it into a digital representation. The values are dependent on
the resolution and limits of the particular device. For example, the Arduino 101
has 12-bit resolution and uses 3.3V on its six analog input pins. So for a
voltage between 0 - 3.3V it returns a digital value of 0 - 4095.

Other Arduino devices typically have 10-bit resolution and some may use 5V as
the upper limit instead.

Hardware may have other analog features such as comparators but so far, we do
not have support for this.

Web IDL
-------

This IDL provides an overview of the interface; see below for
documentation of specific API functions.  Click
[here](Notes_on_WebIDL.md) for an explanation of zephyr.js' WebIDL
conventions.

```javascript
// require returns an AIO object
// var aio = require('aio');

[ReturnFromRequire]
interface AIO {
    AIOPin open(AIOInit init);
};

dictionary AIOInit {
    (unsigned long or string) pin;
};

interface AIOPin {
    unsigned long read();
    void readAsync(ReadCallback callback);  // TODO: change to return a promise
    void on(string eventType, ReadCallback callback);
    void close();
};

callback ReadCallback = void (unsigned long value);
```

AIO API
-------
### aio.open(AIOInit)
* 'AIOInit' *object*  The AIOInit object has a single field called "pin"
  that represents the name of the pin (either an integer or a string,
  depending on the board).

When setting the pin number, you can either use a raw
number for your device or use the board support module such as
[Arduino 101](./boards/arduino_101.md) or [K64F](./boards/frdm_k64f.md) to
specify a named pin.

Returns an AIOPin object that be used to read values from the pin.

AIOPin API
----------
### pin.read()

Returns the latest reading from the pin (an unsigned integer). Blocks until it gets the result.

### pin.readAsync(ReadCallback)
* 'ReadCallback' *callback* User-provided callback function that takes
  a single unsigned integer and has no return value.

Pass a function for `ReadCallback` that will be called later when the result is
obtained.

*WARNING: Making an async call like this allocates some memory while the call
is pending; if async calls are issued faster than they are fulfilled,
the system will
eventually run out of memory - pretty soon on these small devices. So the best
practice would be to have only a small, fixed number pending at
any given time.*

*NOTE: This function will probably be replaced with a version that instead
returns a promise.*

### pin.on(eventType, ReadCallback)
* 'eventType' *string* Type of event; currently, the only supported
  type is "change".
* 'ReadCallback' *callback* User-provided callback function that takes
  a single, unsigned integer and has no return value; can be null. 

The callback function is called any time the analog voltage changes. (At the moment,
it actually gets called periodically even when it hasn't changed.) When null is
passed for the change event, the previously registered callback will be
discarded and no longer called.

### pin.close()

Closes the AIOPin. Once it is closed, all registered event handlers will no
longer be called.

Sample Apps
-----------
* [AIO sample](../samples/AIO.js)
* [Arduino Read Analog Voltage sample](../samples/arduino/basics/ReadAnalogVoltage.js)
* [Arduino Analog Read Serial sample](../samples/arduino/basics/AnalogReadSerial.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
