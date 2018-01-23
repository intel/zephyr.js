ZJS Basic Board Support
=======================

* [Introduction](#introduction)
* [Full Pin Specification](#full-pin-specification)
* [Sample Apps](#sample-apps)

Introduction
------------
Unless a specific board support module has been created for your board, you will
need to use ZJS "Basic Board Support" to use GPIO, AIO, or PWM features.

Full Pin Specification
----------------------
Using basic board support means that there are no "friendly names" predefined
for your board such as "IO3" or "A2". Instead, you will need to specify the
device name and pin number that Zephyr expects. The format these are provided
in is called a full pin specification and that simply means the device name
followed by a '.' and then the pin/channel number.

For example, GPIO devices in Zephyr are often named "GPIO_0", "GPIO_1", and so
on -- one per GPIO controller. For STM32 boards, on the other hand, they are
named 'GPIOA', 'GPIOB', etc. Then the pin numbers will depend on the platform
but you can probably find them by looking up the `pinmux.c` file for your board
in Zephyr.

On a board without specific ZJS support like the Nucleo F411RE (though we'll
probably add that soon!), you would need to refer to the "D6" pin (aka PB_10)
with the name "GPIOB.10".

These full pin specifications can be used on boards with "friendly" name support
as well. So for example "GPIO_0.17" refers to pin IO3 on Arduino 101, while
"GPIO_3.1" refers to pin "PTD1" or "D13" on the FRDM-K64F.

The same kind of mapping would need to be done for AIO pins (usually the device
names are e.g. "ADC_0") and PWM pins (e.g. "PWM_0"). There you will have to
determine what the number is that the device expects for pin/channel number.
Hopefully Zephyr samples or contributors can point you in the right direction.
You can always ask for help on #zjs or #zephyrproject on Freenode IRC.

The pin number for AIO is what you would set as the `channel_id` in the
`adc_seq_entry` to call `adc_read` in Zephyr. The pin number for PWM is the
`pwm` number you would pass to `pwm_pin_set_cycles` in Zephyr.

To create a "friendly" board support package for a new board, you would just
need to make a new .h file in `src/boards` similar to the ones there and a new
file in `docs/boards` that corresponds to it. This is a great way to make your
first valuable contribution to ZJS!

Sample Apps
-----------
* [GPIO Inputs test](../../samples/tests/GPIO-Inputs.js)
* [GPIO Outputs test](../../samples/tests/GPIO-Outputs.js)
