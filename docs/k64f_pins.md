ZJS Board Support for FRDM-K64F
===============================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The k64f_pins module provides named aliases to pins available on the
FRDM-K64F. The third diagram on [this page]
(https://developer.mbed.org/platforms/FRDM-K64F/)
can show you where the pins are located, since they are not identified on the
board itself.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns an K64FPins object
// var pins = require('k64f_pins');

[NoInterfaceObject]
interface K64FPins {
    attribute unsigned long D0;
    attribute unsigned long D1;
    attribute unsigned long D2;
    attribute unsigned long D3;
    attribute unsigned long D4;
    attribute unsigned long D5;
    attribute unsigned long D6;
    attribute unsigned long D7;
    attribute unsigned long D8;
    attribute unsigned long D9;
    attribute unsigned long D10;
    attribute unsigned long D11;
    attribute unsigned long D12;
    attribute unsigned long D13;
    attribute unsigned long D14;
    attribute unsigned long D15;

    attribute unsigned long LEDR;
    attribute unsigned long LEDG;
    attribute unsigned long LEDB;

    attribute unsigned long SW2;
    attribute unsigned long SW3;

    attribute unsigned long PWM0;
    attribute unsigned long PWM1;
    attribute unsigned long PWM2;
    attribute unsigned long PWM3;
    attribute unsigned long PWM4;
    attribute unsigned long PWM5;
    attribute unsigned long PWM6;
    attribute unsigned long PWM7;
    attribute unsigned long PWM8;
    attribute unsigned long PWM9;

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

The FRDM-K64F has 16 general purpose I/O pins, D0 - D15.

D3 and D5 are used for JTAG communication by default in Zephyr. You can disable
this by setting CONFIG_PRESERVE_JTAG_IO_PINS=n in prj.conf, and then use them
as other GPIOs. For board revisions up to 'D", the same applies to pin D8.

D14 and D15 can be used as GPIO inputs but not outputs currently.

The rest (D0 - D2, D4, D6 - D14) can all be used as either GPIO inputs or
outputs.

### LEDs

The FRDM-K64F has an onboard RGB LED which can be controlled through three
different GPIO outputs for the red, green, and blue components.

LEDR controls the red portion, LEDG the green portion, and LEDB the blue
portion. They are all active high.

### Switches

The FRDM-K64F has three onboard switches labeled SW2, SW3, and RESET.

The SW2 switch can be used as a GPIO input.

The SW3 switch is defined but does not seem to work currently.

The RESET switch cannot be used as an input.

### PWM Pins

The FRDM-K64F has ten pins that can be used as PWM output, PWM0 - PWM9. These
are defined but support has not been added yet.

### AIO Pins

The FRDM-K64F has six analog input pins, A0 - A5. These are defined but support
has not been added yet.

Sample Apps
-----------
* [Blink-k64f sample](../samples/arduino/basics/Blink-k64f.js)
* [ButtonLEDs-k64f sample](../samples/ButtonLEDs-k64f.js)
