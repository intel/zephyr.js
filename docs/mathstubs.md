ZJS API for MathStubs
========================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
"MathStubs" module implements a subset of the Math libary's functions such
as random().  This module is served as a replacement for the JerryScript's Math
libary when you only need certain math functions without the need to import
Math so your application can be smaller, since enabling the whole Math
libary will increase your ROM size by at least 30k.  Currently, only random()
is implemented, but feel free to extend it with other useful Math functions like
round(), floor(), etc.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
double MathStubs.random();
```

API Documentation
-----------------
### random
`double random();`

Returns a floating-point, pseudo-random number between 0 (inclusive)and 1 (exclusive).

Sample Apps
-----------
* [Random sample](../samples/Random.js)
