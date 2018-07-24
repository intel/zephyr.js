Zephyr.js API for WebUSB
========================

* [Introduction](#introduction)
* [Class: WebUSB](#webusb-api)
  * [webusb.setURL(url)](#webusbseturlurl)
  * [webusb.write(buffer)](#webusbwritebuffer)
  * [Event: 'read'](#event-read)
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

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a webusb object
// var webusb = require('webusb');<p>[ReturnFromRequire, ExternalInterface=(Buffer)]
interface webusb {
    void setURL(string url);
    void write(Buffer buf);
};
</pre>
</details>

webusb API
----------

### webusb.setURL(url)
* `url` *string* Should begin with "https://" for Chrome to accept it
and display a notification. Other URLs are valid in terms of the protocol but
will have no user-visible effect in Chrome.

### webusb.write(buffer)
* `write` *Buffer*  Writes the data in `buffer` to the WebUSB TX line. By default, at most 511 bytes can be pending at one time, so that is the maximum write size, assuming all previous data has already been flushed out. An error will be thrown on overflow.

### Event: 'read'

* `Buffer` `data`

Emitted when data is received on the WebUSB RX line. The `data` parameter is a
`Buffer` with the received data.

Sample Apps
-----------
* [WebUSB sample](../samples/WebUSB.js)
