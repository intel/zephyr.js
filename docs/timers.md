ZJS API for Timers
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: Timers](#timers-api)
  * [timers.setInterval(func, delay, args_for_func)](#timerssetintervalfunc-delay-args_for_func)
  * [timers.setTimeout(func, delay, args_for_func)](#timerssettimeoutfunc-delay-args_for_func)
  * [timers.clearInterval(intervalID)](#timersclearintervalintervalid)
  * [timers.clearTimeout(timeoutID)](#timerscleartimeouttimeoutid)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides the familiar setTimeout and setInterval interfaces. They are always
available.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>
// require returns a Timers object
// var timers = require('timers');
[ReturnFromRequire]
interface Timers {
    intervalID setInterval(TimerCallback func, unsigned long delay, any... args_for_func);
    timeoutID setTimeout(TimerCallback func, unsigned long delay, any... args_for_func);
    void clearInterval(long intervalID);
    void clearTimeout(long timeoutID);
};<p>
callback TimerCallback = void (any... callback_args);
<p>typedef long timeoutID;
typedef long intervalID;</pre>
</details>

Timers API
----------
### timers.setInterval(func, delay, args_for_func)
* `func` *TimerCallback* A callback function that will take the arguments passed in the variadic `args_for_func` parameter.
* `delay` *unsigned long* The `delay` argument is in milliseconds. Currently, the delay resolution is about 10 milliseconds, and if you choose a value less than that it will probably fail.
* `args_for_func` *any* The user can pass an arbitrary number of additional arguments that will then be passed to `func`.
* Returns: an `intervalID` value that can be passed to `clearInterval` to stop the timer.

Every `delay` milliseconds, your callback function will be called.

### timers.setTimeout(func, delay, args_for_func)
* `func` *TimerCallback* A callback function that will take the arguments passed in the variadic `args_for_func` parameter.
* `delay` *unsigned long* The `delay` argument is in milliseconds. Currently, the delay resolution is about 10 milliseconds.
* `args_for_func` *any* The user can pass an arbitrary number of additional arguments that will then be passed to `func`.
* Returns: a `timeoutID` that can be passed to `clearTimeout` to stop the timer.

After `delay` milliseconds, your callback function will be called *one time*.

### timers.clearInterval(intervalID)
* `intervalID` *long* This value was returned from a call to `setInterval`.

That interval timer will be cleared and its callback function
no longer called.

### timers.clearTimeout(timeoutID)
* `timeoutID` *long* This value was returned from a call to `setTimeout`.

The `timeoutID` timer will be cleared and its callback function will not be
called.

Sample Apps
-----------
* [Timers sample](../samples/Timers.js)
* [Spaceship2 sample](../samples/arduino/starterkit/Spaceship2.js)
