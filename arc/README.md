Web Bluetooth Demo ARC
======================

This will be the arc process responding to AIO commands
and read the AIO pin values, it's configured to work with
the X86 AIO module using IPM.

To build and flash to the ARC on the Arduino 101:

```bash
$ make arc
```

(hit master reset to enter DFU mode on the Arduino)
```bash
$ make dfu-arc
```
