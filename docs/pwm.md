ZJS API for Pulse Width Modulation (PWM)
========================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
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

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a PWM object
// var pwm = require('pwm');

[NoInterfaceObject]
interface PWM {
    PWMPin open(PWMInit init);
};

dictionary PWMInit {
    unsigned long channel;
    double period;               // in milliseconds
    double pulseWidth;           // in milliseconds
    string polarity = "normal";  // normal, reverse
};

[NoInterfaceObject]
interface PWMPin {
    void setPeriod(double ms);
    void setPeriodCycles(unsigned long cycles);
    void setPulseWidth(double ms);
    void setPulseWidthCycles(unsigned long cycles);
};
```

API Documentation
-----------------
### PWM.open

`PWMPin open(PWMInit init);`

The `init` object lets you set the channel number. You can either use a raw
number for your device or use the board support module such as
[Arduino 101](./a101_pins.md) or [K64F](./k64f_pins.md) to specify a named pin.
The term 'channel' is used to refer to the fact that PWM controller hardware has
multiple channels, but these are connected to output pins so as the user of the
hardware you will think of them as pins.

The initial `period` and `pulseWidth` can be specified in milliseconds. These
mean the signal will go on (high) for `pulseWidth` milliseconds and then back
off (low) again, every `period` milliseconds.

If you instead want to specify the period and pulseWidth with finer granularity
in hardware cycles, you can use the corresonding setter functions after the
open.

The `polarity` value should flip the signal if set to 'reverse', meaning the
signal will be off (low) for the pulseWidth, and back on (high) for the rest of
the period. *NOTE: This doesn't seem to work currently on Arduino 101.*

The function returns a PWMPin object that can be used to change the period and
pulse width later.

### PWMPin.setPeriod

`void setPeriod(double ms);`

Sets the repeat period for the pulse signal. It is given in milliseconds, so
these can be fractional to provide microsecond timings, etc. The actual
resolution available will depend on the hardware, so the value you provide may
get rounded.
*TODO: We could probably have the period attribute show the actual setting for
the device when it is read back.*

This version of the API is useful when the timing of the pulse matters (e.g.
the 'servo' model of PWM control described in the
[Introduction](#introduction)).

### PWMPin.setPeriodCycles

`void setPeriodCycles(unsigned long cycles);`

Sets the repeat period for the pulse signal, in terms of hardware cycles. One
hardware cycle is the minimum amount of time the hardware supports having the
pulse signal on (high).

This version of the API is useful when the duty cycle is what matters (e.g.
using the 'analog' model of PWM control described in the
[Introduction](#introduction)). For example, a period of 2 with a pulse width of
1 will make an LED at 50% brightness, with no flicker because the changes occur
far faster than visible to the human eye.

### PWMPin.setPulseWidth

`void setPulseWidth(double ms);`

Sets the pulse width for the signal. It is given in milliseconds, so these can
be fractional to provide microsecond timings, etc. The actual resolution
available will depend on the hardware, so the value you provide may get rounded.
*TODO: We could probably have the pulseWidth attribute show the actual
setting for the device when it is read back.*

This version of the API is useful when the timing of the pulse matters (e.g.
the 'servo' model of PWM control described in the
[Introduction](#introduction)).

### PWMPin.setPulseWidthCycles

`void setPulseWidthCycles(unsigned long cycles);`

Sets the pulse width for the signal, in terms of hardware cycles. One hardware
cycle is the minimum amount of time the hardware supports having the pulse
signal on (high).

This version of the API is useful when the duty cycle is what matters (e.g.
using the 'analog' model of PWM control described in the
[Introduction](#introduction)). For example, a period of 2 with a pulse width of
1 will make an LED at 50% brightness, with no flicker because the changes occur
far faster than visible to the human eye.

Sample Apps
-----------
* [PWM sample](../samples/PWM.js)
* [Arduino Fade sample](../samples/arduino/basics/Fade.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
