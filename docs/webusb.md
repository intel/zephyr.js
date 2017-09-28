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
### WebUSB.setURL

`void setURL(string url);`

The `url` string should begin with "https://" in order for Chrome to accept it
and display a notification. Other URLs are valid in terms of the protocol but
will have no user-visible effect currently in Chrome.

Sample Apps
-----------
* [WebUSB sample](../samples/WebUSB.js)
