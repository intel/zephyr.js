ZJS API for General Purpose I/O (GPIO)
======================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The GPIO API supports digital I/O pins. Pins can be configured as inputs or
outputs, with some board-specific limitations.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a GPIO object
// var gpio = require('gpio');

[NoInterfaceObject]
interface GPIO {
    GPIOPin open(GPIOInit init);
    Promise<GPIOPin> openAsync(GPIOInit init);
};

dictionary GPIOInit {
    unsigned long pin;
    boolean activeLow = false;
    string direction = "out";  // in, out
    string edge = "any";       // none, rising, falling, any
    string pull = "none";      // none, up, down
};

[NoInterfaceObject]
interface GPIOPin {
    boolean read();
    void write(boolean value);
    void close();
    attribute ChangeCallback onchange;
};

callback ChangeCallback = void (GPIOEvent);

dictionary GPIOEvent {
    // TODO: probably should add event type here, or else return value directly
    boolean value;
}
```

API Documentation
-----------------
### GPIO.open

`GPIOPin open(GPIOInit init);`

The `init` object lets you set the pin number. You can either use a raw
number for your device or use the board support module such as
[Arduino 101](./a101_pins.md) or [K64F](./k64f_pins.md) to specify a named pin.

The other values are optional. The `activeLow` setting determines whether
high (default) or low means active. When you read or write a boolean value,
true means 'active' and false means 'inactive'.

The `direction` value determines whether the pin is an input ('in') or output
('out', default).

The `edge` value is for input pins and tells whether the `onchange` callback
will be called on the rising edge of the signal, falling edge, or both.

The `pull` value is to enable an internal pullup or pulldown resistor. This
would be used for inputs to provide a default (high or low) when the input is
floating (not being intentionally driven to a particular value).

*NOTE: Zephyr does not currently use this pull setting, at least for Arduino
101. Perhaps there is no hardware support, but in any case it doesn't work. You
can always provide an external resistor for this purpose instead.*

The function returns a GPIOPin object that can be used to read or write the pin.

### GPIO.openAsync

`Promise<GPIOPin> openAsync(GPIOInit init);`

See above for the format of the init object.

This version of the open call is asynchronous and will complete the open action
later and fulfill or reject the promise. The returned Promise object has then()
and docatch() (should be renamed to catch soon) methods you can use to give a
handler for the success and failure cases. This is based on ECMAScript 6
promises but some other functionality like all() is not available at this time.

### GPIOPin.read

`boolean read();`

Returns the current reading from the pin. This is a synchronous function because
it should be nearly instantaneous on the devices we've tested with so far. The
value will be true if the pin is active (high by default, low for a pin
configured active low), false if inactive.

### GPIOPin.write

`void write(boolean value);`

Pass true for `value` to make an output pin active (high by default, low for
a pin configured active low), false to make it inactive.

### GPIOPin.close

`void close();`

Free up resources associated with the pin. The onchange function for this pin
will no longer be called, and the object should not be used for reading and
writing anymore.

### GPIOPin.onchange

`attribute ChangeCallback onchange;`

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
* Using openAsync
  * [Async ButtonLEDs sample](../samples/ButtonLEDsAsync.js)
