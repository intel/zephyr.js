ZJS Board API
=============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: Board](#board-api)
  * [board.name](#boardname)
  * [board.version](#boardversion)
* [Sample Apps](#sample-apps)

Introduction
------------
The Board API provides information about the circuit board ZJS is running on.

The Board API intends to follow the [iot-js-api specification](https://github.com/intel/iot-js-api/tree/master/board),
but both that and ZJS are under a lot of change at the moment.

Web IDL
-------

This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>// require returns the Board API object
// var board = require('board');
[ReturnFromRequire]
interface Board {
    attribute string name5;
    attribute string version;
};</pre>
</details>

Board API
-----------------
### board.name

`string name;`

The name of the board we're running on. Two boards are well-supported by ZJS
at this time: "arduino_101" and "frdm_k64f". Other boards have been brought up
with some minimal testing: "nrf52_pca10040", "arduino_due", and "nucleo_f411re".
When code is run under Linux with the jslinux utility, it currently reports a
board name of "linux (partially simulating arduino_101)".

Any other board will show up with "unknown". Even if the board is unknown, you
can still use the [GPIO API](gpio.md) by consulting Zephyr
documentation for the board's GPIO port names and pin numbers.

### board.version

`string version;`

Currently, this value is always set to "0.1". Stay tuned!

Sample Apps
-----------
* [Simple Board sample](../samples/Board.js)
