Zephyr.js API for UART
======================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class UART](#uart-api)
  * [uart.init(options)](#uartinitoptions)
* [UARTConnection API](#uartconnection-api)
  * [Event: 'read'](#event-read)
  * [uartConnection.write(data)](#uartconnectionwritedata)
  * [uartConnection.setReadRange(min, max)](#uartconnectionsetreadrangemin-max)

Introduction
------------
The UART module supports both read and write capabilities. Writes are
done through the 'write' function, and reads are done via a callback function property that
can be set. Read and write data should be a JavaScript string.

The module can be used on both QEMU and the Arduino 101. When using QEMU, you
can just type directly into the terminal console. For the Arduino 101, UART is
read/written from the serial console just as print does.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>// require returns a UART object
// var uart = require('uart');<p><p>interface UART {
    UARTConnection init(UARTOptions options);
};<p>dictionary UARTOptions {
    string port;
    // long baud = 115200;
    // long dataBits = 8;
    // long stopBits = 1;
    // UARTParity parity = "none";
    // boolean flowControl = false;
};<p>[ExternalInterface=(buffer,Buffer)]
interface UARTConnection: EventEmitter {
    // void close();
    void write(Buffer data);
    void setReadRange(long min, long max);
};<p>enum UARTParity { "none", "event", "odd" }
</pre>
</details>

UART API
--------
### uart.init(options)
* `options` *UARTOptions* The `UARTOptions` object lets you choose the
  UART device/port you would like to initialize. The Arduino 101, for
  example, should be "tty0".
* Returns: UARTConnection interface, described below.

UARTConnection API
------------------

A UARTConnection is an [EventEmitter](./events.md) with the following events:

### Event: 'read'
* `Buffer` `data`

Emitted when data is received on the UART RX line. The `data` parameter is a
`Buffer` with the received data.

### uartConnection.write(data)
* `data` *Buffer* The data to be written.

Write data out to the UART TX line.

### uartConnection.setReadRange(min, max);`
* `min` *long* The minimum number of bytes for triggering the `onread` event.
* `max` *long* The maximum number of bytes for triggering the `onread` event.

Whenever at least the `min` number of bytes is available, a `Buffer` object
containing at most `max` number of bytes is sent with the `onread` event.

Sample Apps
-----------
* [UART sample](../samples/UART.js)
