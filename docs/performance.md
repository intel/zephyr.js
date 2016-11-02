ZJS API for Benchmarking
========================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
"Performance" module implements subset of "High Resolution Time" specification
from W3C, intended primarily for benchmarking purposes. The key point of this
module is that it is very light-weight by implementing just one function
(unlike for example Date object).

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
double now();
```

API Documentation
-----------------
### now

`now();
`

The module exposes just one function, `now()`. It returns current time in
milliseconds, as a floating-point number, since an arbitrary point in
time. It is thus not useful as an absolute value, but subtracting values
from two calls will give a time duration between these two calls.

As the value returned is floating point, it may have higher resolution
than a millisecond. However, the actual resolution depends on a platform
and its configuration. For example, default Zephyr configuration for
many boards provides resolution of only ones or tens of milliseconds.

The intended usage of this function is for benchmarking and other testing
and development needs.


Examples
--------

    var performance = require("performance");

    t = performance.now()
    do_long_operation();
    console.out("Long operation took:", performance.now() - t, "ms");


Sample Apps
-----------
* [Performance module unit test](../tests/test-performance.js)
