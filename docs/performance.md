ZJS API for Benchmarking
========================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Performance API](#performance-api)
  * [performance.now()](#performancenow)
* [Sample Apps](#sample-apps)

Introduction
------------

The "Performance" module implements a subset of the "High Resolution Time"
specification from W3C, intended primarily for benchmarking
purposes. The key point of this module is that it is very light-weight
by implementing just one function (unlike, for example, the Date object).

Web IDL
-------

This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a Performance object
// var ble = require('performance');
[ReturnFromRequire]
interface Performance {
    double now();
};</pre>
</details>


Performance API
---------------
### performance.now()
* Returns: the current time in milliseconds, as a floating-point number.

The "time" returned from this function is the offset since an
arbitrary point in time. It is thus not useful as an absolute value,
but subtracting values from two calls will give a time duration
between these two calls.

As the value returned is a floating point value, it may have higher resolution
than a millisecond. However, the actual resolution depends on a platform
and its configuration. For example, the default Zephyr configuration for
many boards provides resolution of only ones or tens of milliseconds.

The intended use of this function is for benchmarking and other testing
and development needs.

Examples
--------

    var performance = require("performance");

    t = performance.now()
    do_long_operation();
    console.log("Long operation took:", performance.now() - t, "ms");


Sample Apps
-----------
* [Performance module unit test](../tests/test-performance.js)
