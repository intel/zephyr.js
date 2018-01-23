ZJS Board Support for FRDM-K64F
===============================

* [Introduction](#introduction)
* [Available Pins](#available-pins)
* [Sample Apps](#sample-apps)

Introduction
------------
The FRDM-K64F is pretty well supported by ZJS. This page lists the available
pins and how to refer to them in ZJS.

For multi-board support, you can use the [board module](../board.md) to detect
whether you're running on the FRDM-K64F with `board.name === 'frdm_k64f'`.

Available Pins
--------------
It will be helpful to refer to the two diagrams
[here](https://os.mbed.com/platforms/FRDM-K64F/#component-pinout)
when working with the K64F.

### GPIO Pins
The FRDM-K64F has 16 general purpose I/O pins, D0 - D15.

If you look at the diagrams above, the pin D0 in the bottom right corner is
also labeled PTC16. This means it's pin 16 on the 'C' GPIO controller. The full
pin specification for that pin is "GPIO_2.16" (2 because the controllers A-E are
0-indexed, 0 to 4). All the other pins listed with a blue box of the format
PTC16 can also be used as GPIOs. For example, the pins that can also be used as
analog inputs (A0 - A5) also have GPIO names. (These work as GPIOs unless you
have also included the [AIO module](../aio.md).)

You can refer to the pins D0 - D15 with strings "D0" to "D15", numbers 0 to 15,
or strings "0" to "15". For the other pins, you need to use the full
pin specification. *NOTE: we plan to add support for the "PTC16" style names
directly to make this easier.*

### LEDs
The FRDM-K64F has an onboard RGB LED which can be controlled through three
different GPIO outputs for the red, green, and blue components.

LEDR controls the red portion, LEDG the green portion, and LEDB the blue
portion. They are all active low. They can be referred to with "LEDR", "LEDG",
and "LEDB" (or "LED0", "LED1", and "LED2") or their full pin specifications
such as "GPIO_1.22" for red.

These can be treated like other GPIOs, but don't work as inputs.

### Switches
The FRDM-K64F has three onboard switches labeled SW2, SW3, and RESET.

The SW2 switch can be used as a GPIO input. Refer to it with "SW2" or
"GPIO_2.6". The SW3 switch is defined but does not seem to work currently. You
would refer to it with "SW3" or "GPIO_0.4". The RESET switch cannot be used as
an input.

These can be treated like other GPIOs, but don't work as outputs.

### AIO Pins
The FRDM-K64F has six analog input pins, A0 - A5.

Pins A0 - A3 work and you can refer to them with "A0" to "A3" or their full
pin specifications. Pins A4 and A5 do not work, because they apparently use
hardware triggering which isn't supported by the Zephyr driver.

We have also defined A6 and A7 for ZJS. A6 refers to the pin labeled
ADC1_SE18 in a blue label on the diagrams mentioned above, and A7 refers to the
pin labeled DAC0_OUT. These also work as analog inputs.

All the analog pins report a 12-bit digital value, from 0 to 4095.

### PWM Pins
*PWM support for K64F is not enabled yet, but when it is the pins should be
as follows.*

The FRDM-K64F has twelve pins that can be used as PWM output. For ZJS purposes,
we have named them PWM0 - PWM11. These correspond to the purple labels on the
diagrams mentioned above. We've numbered them sequentially starting with the
bottom right (D3/PTA1) upward, and then with A4 and A5 as PWM10 and PWM11.

However, PWM4 (D8) will probably not work even when this feature is enabled,
because the pin name in the schematics doesn't make it clear how it would be
identified to the driver. (Too much detail: this is FTM3_FLT0.)

When support is added, you will be able to refer to these with names like
"PWM0" but also "D3" and "A4" to identify them more clearly based on the
diagram.

Sample Apps
-----------
* [GPIO Inputs test](../../samples/tests/GPIO-Inputs.js)
* [GPIO Outputs test](../../samples/tests/GPIO-Outputs.js)
* [AIO sample](../../samples/AIO.js)
