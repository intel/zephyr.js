ZJS API for Console
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: Console](#console-api)
  * [console.assert(value, [message])](#consoleassertvalue-message)
  * [console.error([data])](#consoleerrordata)
  * [console.warn([data])](#consolewarndata)
  * [console.log([data])](#consolelogdata)
  * [console.info([data])](#consoleinfodata)
  * [console.time(label)](#consoletimelabel)
  * [console.timeEnd(label)](#consoletimeendlabel)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides console APIs which match Node.js' Console module. We
describe them here as there could be minor differences.

Note that the console APIs do not support format specifiers (e.g., %d, %f etc.).

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>
// require returns a Console object
// var console = require('console');
[ReturnFromRequire]
interface Console {
    void assert(boolean value, optional string message);
    void error(optional string data);
    void warn(optional string data);
    void log(optional string data);
    void info(optional string data);
    void time(string label);
    void timeEnd(string label);
};</pre>
</details>

Console API
-----------

### console.assert(value, [message])
* `value` *boolean*
*  `message` *string* Optional message to print.

Assert/throw an error if `value` is false.

### console.error([data])
* `data` *string* Optional message to print.

Prints `data` to `stderr` with newline. (On Zephyr this will just print to stdout).

### console.warn([data])
* `data` *string* Optional message to print.

Alias for `console.error()`

### console.log([data])
* `data` *string* Optional message to print.

Prints `data` to `stdout` with newline.

### console.info([data])
* `data` *string* Optional message to print.

Alias for `console.log()`.

### console.time(label)
* `label` *string* This string is used to reference the timer when calling `console.timeEnd()`.

Starts a timer used to compute the duration of an operation.

### console.timeEnd(label)
* `label` *string* The label identifying the timer started with `console.time()`.

Stops a timer previously started with `console.time()` and prints the resulting time difference to `stdout`.

Sample Apps
-----------
* [Console sample](../samples/tests/Console.js)
