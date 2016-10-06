ZJS Board Support for Arduino 101
=================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The arduino101_pins module provides named aliases to pins available on the
Arduino 101.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns an Arduino101Pins object
// var pins = require('arduino101_pins');

[NoInterfaceObject]
interface Arduino101Pins {
    attribute unsigned long IO2;
    attribute unsigned long IO3;
    attribute unsigned long IO4;
    attribute unsigned long IO5;
    attribute unsigned long IO6;
    attribute unsigned long IO7;
    attribute unsigned long IO8;
    attribute unsigned long IO9;
    attribute unsigned long I10;
    attribute unsigned long I11;
    attribute unsigned long I12;
    attribute unsigned long I13;

    attribute unsigned long LED0;
    attribute unsigned long LED1;
    attribute unsigned long LED2;

    attribute unsigned long PWM0;
    attribute unsigned long PWM1;
    attribute unsigned long PWM2;
    attribute unsigned long PWM3;

    attribute unsigned long A0;
    attribute unsigned long A1;
    attribute unsigned long A2;
    attribute unsigned long A3;
    attribute unsigned long A4;
    attribute unsigned long A5;
};
```

API Documentation
-----------------
### GPIO Pins

The Arduino 101 has 14 general purpose I/O pins, IO0 - IO13.

IO0 and IO1 are used for serial communication and we haven't yet looked into
disabling that to provide them as extra GPIO pins, so these are not available.

IO6 and IO9 are defined but not actually usable as GPIOs currently.

IO3 and IO5 can be used as GPIO inputs but not outputs currently.

The rest (IO2, IO4, IO7, IO8, and IO10 - IO13) can all be used as either GPIO
inputs or outputs.

### LEDs

The Arduino 101 has three onboard LEDs that you can access as additional GPIO
outputs.

LED0 controls an onboard green LED. It is active high, and it is an alias for
IO13, so in other words, LED0 displays the current state of IO13. So don't try
to use both names.

LED1 controls another onboard green LED. It is active *low*.

LED2 controls an onboard red fault LED. It is active *low*.

### PWM Pins

The Arduino 101 has four pins that can be used as PWM output. As usual on
Arduino, these pins are marked on the board by a ~ (tilde) symbol next to the
pins, namely IO3, IO5, IO6, and IO9. The names PWM0 - PWM3 correspond to those
four pin names and can be used interchangeably, depending on your preference.

### AIO Pins

The Arduino 101 has six analog input pins, A0 - A5.

Sample Apps
-----------
* [TwoButtons sample](../samples/TwoButtons.js)
* [PWM sample](../samples/PWM.js)
* [AIO sample](../samples/AIO.js)
