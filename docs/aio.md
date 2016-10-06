ZJS API for Analog I/O (AIO)
============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
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
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns an AIO object
// var aio = require('aio');

[NoInterfaceObject]
interface AIO {
    AIOPin open(AIOInit init);
};

dictionary AIOInit {
    unsigned long pin;
    unsigned long device;  // TODO: remove for now, not used
    string name;  // TODO: remove, not used
    boolean raw;  // TODO: remove, not used
};

[NoInterfaceObject]
interface AIOPin {
    unsigned long read();
    void readAsync(ReadCallback callback);  // TODO: change to return a promise
    void on(string eventType, ReadCallback callback);
    void close();
};

callback ReadCallback = void (unsigned long value);
```

API Documentation
-----------------
### AIO.open

`AIOPin open(AIOInit init);`

The `init` object lets you set the pin number. You can either use a raw
number for your device or use the board support module such as
[Arduino 101](./a101_pins.md) or [K64F](./k64f_pins.md) to specify a named pin.

Use the AIOPin object returned to read values from the pin.

### AIOPin.read

`unsigned long read();`

Returns the latest reading from the pin. Blocks until it gets the result.

### AIOPin.readAsync

`void readAsync(ReadCallback callback);`

Pass a function for `callback` that will be called later when the result is
obtained.

*WARNING: Making an async call like this allocates some memory while the call
is pending, so if you issue them faster than they are fulfilled, you will
eventually run out of memory - pretty soon on these small devices. So the best
practice would be to make sure you only have a small, fixed number pending at
any given time.*

*NOTE: This function will probably be replaced with a version that instead
returns a promise.*

### AIOPin.on

`void on(string eventType, ReadCallback callback);`

Currently, the only supported `eventType` is 'change', and `callback` should
be either a function or null. When a function is passed for the change event,
the function will be called any time the analog voltage changes. (At the moment,
it actually gets called periodically even when it hasn't changed.) When null is
passed for the change event, the previously registered callback will be
discarded and no longer called.

### AIOPin.close

`void close();`

Closes the AIOPin. Once it is closed, all event handlers registered will no
longer be called.

Sample Apps
-----------
* [AIO sample](../samples/AIO.js)
* [Arduino Read Analog Voltage sample](../samples/arduino/basics/ReadAnalogVoltage.js)
* [Arduino Analog Read Serial sample](../samples/arduino/basics/AnalogReadSerial.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
