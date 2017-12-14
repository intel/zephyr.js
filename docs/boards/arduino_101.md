ZJS Board Support for Arduino 101
=================================

* [Introduction](#introduction)
* [Available Pins](#available-pins)
* [Sample Apps](#sample-apps)

Introduction
------------
The Arduino 101 is pretty much fully supported by ZJS. Two exceptions are that
certain GPIOs that are accessible only from the ARC core are not available, and
there are analog comparator pins that are not supported.

For multi-board support, you can use the [board module](../board.md) to detect
whether you're running on the Arduino 101 with `board.name === 'arduino_101'`.

Available Pins
--------------
### GPIO
The Arduino 101 has 14 general purpose I/O pins exposed, IO0 - IO13.

IO0 and IO1 are used for serial communication and we haven't yet looked into
disabling that to provide them as extra GPIO pins, so these are not available.

IO6 and IO9 are defined but not actually usable as GPIOs currently. They can
only be accessed from the ARC side which we do not support.

IO3 and IO5 can be used as GPIO inputs but not outputs currently.

The rest (IO2, IO4, IO7, IO8, and IO10 - IO13) can all be used as either GPIO
inputs or outputs. *FIXME: However, at the time of this writing IO8, IO10, and
IO11 do not seem to be working; they used to.*

There are different ways to refer to a pin like IO3. The easiest way is the
string 'IO3', but you can also use the string '3' or number 3, and finally you
can use the full pin specification 'GPIO_0.17', but this requires you to look
up the mapping between logical pins and GPIO numbers. The only way we know to do
this is consulting Zephyr's `boards/x86/arduino_101/pinmux.c` file. The Arduino
101 only has one GPIO controller so the device is always 'GPIO_0' for full pin
specifications like this. But if you use the friendly forms, you don't need to
worry about that.

Note, SPI bus support uses IO10 - IO13 so when that is enabled, those pins are
not available. The flash filesystem uses SPI, so these GPIOs are not available
when you use ashell or filesystem APIs.

### LEDs
The Arduino 101 has three onboard LEDs that you can access as additional GPIO
outputs.

LED0 controls an onboard red fault LED. It is active *low*.

LED1 controls an onboard green LED. It is active *low*.

LED2 controls another onboard green LED. It is active high, and it is an alias
for IO13, so in other words, LED0 displays the current state of IO13. So don't
try to use both names.

Note, if SPI is enabled this reconfigures the GPIO that controls LED2 and it
will not be available. The flash filesystem uses SPI, so LED2 is not available
when you use ashell or filesystem APIs.

You can refer to the pins for these LEDs with the strings 'LED0', 'LED1', and
'LED2'.

### AIO
The Arduino 101 has six analog input pins, A0 - A5.

You can refer to these pins with strings 'A0' to 'A5', 'AD0' to 'AD5', numbers
0 to 5, strings '0' to '5', or full pin specification like 'ADC_0.10' but you
will have to look up the mapping in the Zephyr `pinmux.c` file. There is only
one analog input controller, ADC_0.

### PWM
The Arduino 101 has four pins that can be used as PWM outputs. As usual on
Arduino, these pins are marked on the board by a ~ (tilde) symbol next to the
pins, namely IO3, IO5, IO6, and IO9.

You can refer to these pins with strings 'PWM0' to 'PWM3', numbers 0 to 3,
strings '0' to '3', with the IO pin names 'IO3', 'IO5', 'IO6', and 'IO9', or
full pin specification 'PWM_0.0' to 'PWM_0.3'. There is only one PWM
controller, PWM_0.

Sample Apps
-----------
* [GPIO Inputs test](../../samples/tests/GPIO-Inputs.js)
* [GPIO Outputs test](../../samples/tests/GPIO-Outputs.js)
* [AIO sample](../../samples/AIO.js)
* [PWM sample](../../samples/PWM.js)
