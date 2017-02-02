ARC Support Image
=================

This directory contains a Zephyr application to run on the ARC side of the
Quark SE chip to handle interfacing with hardware that is only accessible
from that side, such as analog input pins, I2C bus, sensors, etc.

The X86 image communicates with it over IPM.

This image is built and flashed to the Arduino 101 as part of the same commands
that build the X86 image:

```bash
$ make BOARD=arduino_101
```

(The BOARD argument is optional because this is the default currently.)

Then hit the master reset button to enter DFU mode on the Arduino, and after a
couple seconds type:

```bash
$ make dfu
```

This will flash both the X86 and ARC images.
