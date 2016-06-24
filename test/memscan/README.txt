Title: memscan

Description:
A simple app to test memory mapping.

--------------------------------------------------------------------------------

Building and Running Project:

This microkernel project outputs to the console.  It can be built and executed
on QEMU as follows:

    make pristine && make BOARD=qemu_x86
    make qemu

It can be built and flashed to the Arduino 101 as follows:

    make pristine && make BOARD=arduino_101 ARCH=x86
    make BOARD=arduino_100 flash

If the serial console is connected, the output should appear there.

--------------------------------------------------------------------------------

Troubleshooting:

Problems caused by out-dated project information can be addressed by
issuing one of the following commands then rebuilding the project:

    make clean          # discard results of previous builds
                        # but keep existing configuration info
or
    make pristine       # discard results of previous builds
                        # and restore pre-defined configuration info

--------------------------------------------------------------------------------

Sample Output:

Microkernel Memory Scan
-----------------------
Allocated 45824B successfully!
Writing all zeroes succeeded.
Writing all ones succeeded.
Writing varying values succeeded.
