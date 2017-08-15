Zephyr.js API for UART
======================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The UART module supports both read and write capabilities. Write is achieved
with a 'write' function, and read is done via a callback function property that
can be set. Read and write data should be a JavaScript string.

The module can be used on both QEMU and the Arduino 101. When using QEMU, you
can just type directly into the terminal console. For the Arduino 101, UART is
read/written from the serial console just as print does.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions. Commented out properties or functions are not
implemented because the feature is not available on Zephyr.

```javascript
// require returns a UART object
// var uart = require('uart');

enum UARTParity { "none", "event", "odd" }

interface UART {
    UARTConnection init(UARTOptions options);
};

dictionary UARTOptions {
    string port;
    // number baud = 115200;
    // number dataBits = 8;
    // number stopBits = 1;
    // UARTParity parity = "none";
    // boolean flowControl = false;
};

[NoInterfaceObject]
interface UARTConnection: EventEmitter {
    // void close();
    void write(Buffer data);
    void setReadRange(number min, number max);
};
```

API Documentation
-----------------
## UART interface

### UART.init

`UARTConnection init(UARTOptions options);`

The `options` object lets you choose the UART device/port you would like to
initialize. The Arduino 101, for example, should be "tty0".

## UARTConnection interface
UARTConnection is an [EventEmitter](./events.md) with the following events:

### Event: 'read'

* `Buffer` `data`

Emitted when data is received on the UART RX line. The `data` parameter is a
`Buffer` with the recieved data.

### UARTConnection.write

`void write(Buffer data);`

Write data out to the UART TX line. `data` is a string that will be written.

### UARTConnection.setReadRange

`void setReadRange(number min, number max);`

Set the minimum and maximum number of bytes for triggering the `onread` event.
Whenever at least the `min` number of bytes is available, a `Buffer` object
containing at most `max` number of bytes is sent with the `onread` event.

Sample Apps
-----------
* [UART sample](../samples/UART.js)
