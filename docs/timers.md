ZJS API for Timers
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides the familiar setTimeout and setInterval interfaces. They are always
available.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
intervalID setInterval(TimerCallback func, unsigned long delay, optional arg1, ...);
timeoutID setTimeout(TimerCallback func, unsigned long delay, optional arg1, ...);
void clearInterval(intervalID);
void clearTimeout(timeoutID);

callback TimerCallback = void (optional arg1, ...);
```

API Documentation
-----------------
### setInterval

`intervalID setInterval(TimerCallback func, unsigned long delay, optional arg1, ...);
`

The `func` argument is a callback function that should expect whatever arguments
you pass as arg1, arg2, and so on.

The `delay` argument is in milliseconds. Currently, the delay resolution is
about 10 milliseconds and if you choose a value less than that it will probably
fail.

Any additional arguments such as `arg1` will be passed to the callback you
provide. They can be whatever type you wish.

Every `delay` milliseconds, your callback function will be called. An
`intervalID` will be returned that you can save and pass to clearInterval later
to stop the timer.

### setTimeout

`timeoutID setTimeout(TimerCallback func, unsigned long delay, optional arg1, ...);`

The `func` argument is a callback function that should expect whatever arguments
you pass as arg1, arg2, and so on.

The `delay` argument is in milliseconds. Currently, the delay resolution is
about 10 milliseconds.

Any additional arguments such as `arg1` will be passed to the callback you
provide. They can be whatever type you wish.

After `delay` milliseconds, your callback function will be called *one time*. A
`timeoutID` will be returned that you can save and pass to clearTimeout later
to stop the timer.

### clearInterval

`void clearInterval(intervalID);`

The `intervalID` should be what was returned from a previous call to
`setInterval`. That interval timer will be cleared and its callback function
no longer called.

### clearTimeout

`void clearTimeout(timeoutID);`

The `timeoutID` should be what was returned from a previous call to
`setTimeout`. That timer will be cleared and its callback function will not be
called.

Sample Apps
-----------
* [Timers sample](../samples/Timers.js)
* [Spaceship2 sample](../arduino/starterkit/Spaceship2.js)
