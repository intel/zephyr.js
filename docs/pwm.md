ZJS API for Pulse Width Modulation (PWM)
========================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: PWM](#pwm-api)
  * [pwm.open(init)](#pwmopeninit)
* [Class: PWMPin](#pwmpin-api)
  * [pin.setCycles(period, pulseWidth)](#pinsetcyclesperiod-pulsewidth)
  * [pin.setMilliseconds(periodMS, pulseWidthMS)](#pinsetmillisecondsperiodms-pulsewidthms)
* [Sample Apps](#sample-apps)

Introduction
------------
The PWM API supports pins with dedicated pulse width modulation support. This
uses programmable hardware to generate a repeated signal, a squarewave pulse,
where the signal goes up and down at specific times. For example, you can tell
the PWM hardware to pulse pin IO3 for 1ms every 3ms, and it will do that
indefinitely until you tell it otherwise, without further software intervention.

There are two ways to control hardware devices with PWM. Some devices, like
servos, use different pulse widths as control commands. One example is a servo
that will turn 90 degrees to the left with a 1ms pulse and 90 degress to the
right with a 2ms pulse. Anything in between will turn it a proportional,
variable amount. So a 1.25ms pulse will turn it 45 degrees left of center, and a
1.5ms pulse will recenter it. For such a servo, the period is not as important.
It might be 5ms or 20ms and have the same effect.

The other way to use PWM to control a device is to think about the duty cycle;
that is, what percentage of the time is the signal "on". The Arduino API calls
this "analogWrite" which is a bit of a misnomer: the signal is still a digital
squarewave. But if you control, for example, an LED with a PWM, and give it a
duty cycle of 50% (such as 1ms on, 1ms off), it will glow at half brightness.
If you give it a duty cycle of 10% (such as 1ms on, 9ms off), it will glow at
10% brightness. So as with the Arduino API, you can think of this as a way to
set an "analog" brightness, not just "on" or "off".

The PWM API intends to follow the [iot-js-api specification](https://github.com/intel/iot-js-api/tree/master/board/pwm.md).

Web IDL
-------

This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a PWM object
// var pwm = require('pwm');
[ReturnFromRequire]
interface PWM {
    PWMPin open((long or string or PWMInit) init);
};<p>
dictionary PWMInit {
    (long or string) pin;
    boolean reversePolarity = false;
};<p>interface PWMPin {
    void setCycles(unsigned long period, unsigned long pulseWidth);
    void setMilliseconds(double period, double pulseWidth);
};</pre>
</details>

PWM API
-------
### pwm.open(init)

* `init` *long of string or PWMInit* A numerical argument indicates
a pin channel number. If the argument is a string, it is a pin
name. Otherwise, it must be a `PWMInit` object.
* Returns: a PWMPin object that can be used to set the period and
pulse width.

The `init` object lets you set the pin channel number with the `pin`
property.  You can use a pin name like "PWM_0.2", where "PWM_0" is the
name of a Zephyr pwm controller device for your board and 210 is the
pin/channel number. This will work on any board as long as you find
the right values in the Zephyr documentation. For boards with specific
ZJS support (currently: Arduino 101 and FRDM-K64F), you can use
friendly names.

*Arduino 101*
For the A101, you can use numbers 0-3, strings "PWM0" through "PWM3", or the
corresponding digital pin names "IO3", "IO5", "IO6", and "IO9".

*FRDM-K64F*
For the K64F, you can use numbers 0-11, strings "D3", "D5" through "D13",
and "A4" and "A5".

The term 'channel' refers to multiple channels on the PWM controller
hardware, but these are connected to output pins, so the hardware user
should think of them as pins.

The `reversePolarity` value should flip the signal if set to 'reverse', meaning
the signal will be off (low) for the pulseWidth, and on (high) for the
rest of the period.

PWMPin API
----------

### pin.setCycles(period, pulseWidth)
* `period` *unsigned long*
* `pulseWidth` *unsigned long*

Sets the repeat period and pulse width for the signal, in terms of hardware
cycles. One hardware cycle is the minimum amount of time the hardware supports
having the pulse signal on (high).

Throws an error if pulseWidth is greater than period.

This version of the API is useful when the duty cycle is what matters (e.g.,
using the 'analog' model of PWM control described in the
[Introduction](#introduction)). For example, a period of 20 with a pulse width
of 10 will make an LED at 50% brightness, with no flicker because the changes
occur far faster than visible to the human eye.

### pin.setMilliseconds(periodMS, pulseWidthMS)
* `periodMS` *double*  Signal repeat period.
* ` pulseWidthMS` *double* Signal pulse width.

Sets the repeat period and pulse width for the signal. The values are given in
milliseconds, so these can be fractional to provide microsecond
timings, for example.
The actual resolution available will depend on the hardware, so the value you
provide may get rounded.
*TODO: We could probably have the period attribute show the actual setting for
the device when it is read back.*

Throws an error if pulseWidth is greater than period.

This version of the API is useful when the timing of the pulse matters (e.g.,
the 'servo' model of PWM control described in the
[Introduction](#introduction)).

Sample Apps
-----------
* [PWM sample](../samples/PWM.js)
* [Arduino Fade sample](../samples/arduino/basics/Fade.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
