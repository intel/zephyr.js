Zephyr.js API for WebUSB
========================

* [Introduction](#introduction)
* [Class: WebUSB](#class-webusb)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The WebUSB module supports advertising the device as a WebUSB device when the
USB port is connected to a PC. Currently, this only works on Arduino 101 as it
is the only board with a USB driver in the Zephyr tree.

The API allows you to set the URL that will be suggested to the browser for
connecting to your device.

When you connect your device to a Linux PC or Mac with Chrome >= 60 running, it
will give a notification that the device would like you to visit the URL you've
set. (Windows currently prevents this from working, I believe.)

API Documentation
-----------------
### Event: 'read'

* `Buffer` `data`

Emitted when data is received on the WebUSB RX line. The `data` parameter is a
`Buffer` with the received data.

### WebUSB.setURL

`void setURL(string url);`

The `url` string should begin with "https://" in order for Chrome to accept it
and display a notification. Other URLs are valid in terms of the protocol but
will have no user-visible effect in Chrome.

### WebUSB.write

`void write(Buffer buffer);`

Writes the data in `buffer` to the WebUSB TX line. By default at most 511 bytes
can be pending at one time, so that is the maximum write size, assuming all
previous data had already been flushed out. An error will be thrown on overflow.

Sample Apps
-----------
* [WebUSB sample](../samples/WebUSB.js)
