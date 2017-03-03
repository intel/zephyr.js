ZJS API for Console
==================

* [Introduction](#introduction)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides console API's which match Node.js' Console module. We describe them here as there could
potentially be minor differences.

Note that the console API's do not support format specifiers (e.g. %d, %f etc.).

API Documentation
-----------------

### assert
`void assert(boolean value, optional string message);`

Assert/throw an error if `value` is false.

### error
`void error(optional string data);`

Prints `data` to `stderr` with newline. (On Zephyr this will just print to stdout).

### warn
`void warn(optional string data);`

Alias for `console.error()`

### log
`void log(optional string data);`

Prints `data` to `stdout` with newline.

### info
`void info(optional string data);`

Alias for `console.log()`.

### time
`void time(string label);`

Starts a timer used to compute the duration of an operation. The `label` string is used
to reference the timer when calling `console.timeEnd()`.

### timeEnd
`void timeEnd(string label);`

Stops a timer previously started with `console.time()` and prints the resulting time
difference to `stdout`.

Sample Apps
-----------
* [Console sample](../samples/Console.js)
