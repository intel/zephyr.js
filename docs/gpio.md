ZJS API for General Purpose I/O (GPIO)
======================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class GPIO](#gpio-api)
  * [GPIO.open(init)](#gpioopeninit)
* [Class GPIOPin](#gpiopin-api)
  * [pin.read()](#pinread)
  * [pin.write()](#pinwritevalue)
  * [pin.close()](#pinclose)
  * [pin.onchange](#pinonchange)
* [Sample Apps](#sample-apps)

Introduction
------------
The GPIO API supports digital I/O pins. Pins can be configured as inputs or
outputs, with some board-specific limitations.

The GPIO API intends to follow the [iot-js-api specification](https://github.com/intel/iot-js-api/tree/master/board/gpio.md),
but both that and ZJS are under a lot of change at the moment.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a GPIO object
// var gpio = require('gpio');
[ReturnFromRequire]
interface GPIO {
    GPIOPin open( (long or string or GPIOInit) init);
};<p>
dictionary GPIOInit {
    (long or string) pin;
    boolean activeLow = false;
    GPIOMode  mode =  "out";
    GPIOEdge  edge =  "none";
    GPIOState state = "none";
};<p>interface GPIOPin {
    long read();
    void write(long value);
    void close();
    attribute ChangeCallback onchange;
};<p>callback ChangeCallback = void (GPIOEvent event);<p>dictionary GPIOEvent {
    long value;
};<p>enum GPIOMode  { "out", "in" };
enum GPIOEdge  { "none", "rising", "falling", "any" };
enum GPIOState { "none", "up", "down" };</pre>
</details>

GPIO API
--------
### gpio.open(init)
* `init` *long or string or GPIOInit* If the argument is a number, it is a pin number. If it is a
string, it is a pin name. Otherwise, it must be a GPIOInit object.
* Returns: a GPIOPin object that can be used to read or write the pin.

If the pin number or name is valid for the given board, the call will succeed.
You can use a pin name like "GPIO_0.10" where "GPIO_0" is the name of a Zephyr
gpio port device for your board and 10 is the pin number. This will work on any
board as long as you find the right values in Zephyr documentation. But for
boards with specific ZJS support, you can use friendly names. Currently, this
means Arduino 101 and FRDM-K64F. For the A101, you can use numbers 0-13 or
strings "IO0" through "IO13", as well as "LED0" through "LED2". For K64F, you
can use numbers 0-15 or strings "D0" through "D15", as well as "LEDR", "LEDG",
and "LEDB" for the RGB LED, and "SW2" and "SW3" for onboard switches.

The GPIOInit object can take a string or number as the pin argument,
and all of the rest of the fields are optional. The `activeLow`
setting determines whether high (default) or low means active. When
you read or write a boolean value, true means 'active' and false means
'inactive'.

The `mode` value determines whether the pin is an input ('in') or output
('out').

The `edge` value is for input pins and tells whether the `onchange` callback
will be called on the rising edge of the signal, falling edge, or both.

The `state` value is useful when the architecture has an internal
pullup or pulldown resistor. This would be used for inputs to provide
a default (high or low) when the input is floating (not being
intentionally driven to a particular value).

*NOTE: When we last checked, Zephyr did not use this state setting, at least for
Arduino 101. Perhaps there is no hardware support, but in any case, it didn't
work. You can always provide an external resistor for this purpose instead.*

GPIOPin API
-----------
### pin.read()
* Returns: the current reading from the pin.

This is a synchronous function, because it is nearly
instantaneous on the devices we've tested with so far. The value will
be 1 if the pin is active (high by default, low for a pin configured
active low), 0 if inactive.

### pin.write(value)
* `value` *long*  Pass 1 for `value` to make an output pin active
(high by default, low for a pin configured active low), 0 to make it inactive.

### pin.close()

Free up resources associated with the pin. The onchange function for this pin
will no longer be called, and the object should not be used for reading and
writing anymore.

### pin.onchange

* `onchange` *ChangeCallback*

Set this attribute to a function that will receive events whenever the pin
changes according to the edge condition specified at pin initialization. The
event object contains a `value` field with the current pin state.

Sample Apps
-----------
* GPIO input only
  * [Arduino DigitalReadSerial sample](../samples/arduino/basics/DigitalReadSerial.js)
* GPIO output only
  * [Arduino Blink sample](../samples/arduino/basics/Blink.js)
  * [RGB LED sample](../samples/RGB.js)
* GPIO input/output
  * [AutoButton sample](../samples/AutoButton.js)
  * [Arduino Button sample](../samples/arduino/digital/Button.js)
  * [ButtonLEDs sample](../samples/ButtonLEDs.js)
  * [TwoButtons sample](../samples/TwoButtons.js)
