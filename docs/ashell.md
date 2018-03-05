'A Shell' Command line interface to run JavaScript on demand
============================================================

* [Introduction](#introduction)
* [Compilation](#compilation)
* [Connect](#connect)
* [Commands](#commands)

Introduction
------------
This shell focus is on creating an interface to be able to run commands and
arbitrary JavaScript directly from the USB port of a Zephyr enabled device.

Although the shell would look like a common Unix Bash, it is heavily limited
due the constraints of the micro-controller.

NOTE: Because the filesystem module is used by ashell, the SPI module is also
included. This means that on the Arduino 101, pins 10 - 13 are set as SPI and
can't be used as GPIO pins.

Compilation
------------

This command will enable the shell compilation for use with the IDE:
```bash
$ make ide
```

To use the shell via the command line use:
```bash
$ make ashell
```

Dev mode will append all the functionality from ZJS, so it might not fit in ROM.

Connect
-------

WebUSB support was added to the ashell to allow the user to upload JS code
directly from the browser IDE to Zephyr.js device for execution. Follow the
below instructions to connect to the device from the browser IDE directly.

* Start Google Chrome 59 or later on the host PC.

* **On Ubuntu and Fedora:**
1. Create udev rules to allow Chrome to open the WebUSB enabled devices and
   also prevent ModemManager interfering with those devices by adding the following
   lines in /etc/udev/rules.d/99-webusb.rules

* **Arduino 101**
>     SUBSYSTEM=="tty", ENV{ID_VENDOR_ID}=="8086", ENV{ID_MODEL_ID}=="f8a1", MODE="0666", ENV{ID_MM_DEVICE_IGNORE}="1", ENV{ID_MM_CANDIDATE}="0"
>     SUBSYSTEM=="usb", ATTR{idVendor}=="8086", ATTR{idProduct}=="f8a1", MODE="0666" ENV{ID_MM_DEVICE_IGNORE}="1"

* **Default VID/PID**
>     SUBSYSTEM=="tty", ENV{ID_VENDOR_ID}=="dead", ENV{ID_MODEL_ID}=="beef", MODE="0666", ENV{ID_MM_DEVICE_IGNORE}="1", ENV{ID_MM_CANDIDATE}="0"
>     SUBSYSTEM=="usb", ATTR{idVendor}=="dead", ATTR{idProduct}=="beef", MODE="0666" ENV{ID_MM_DEVICE_IGNORE}="1"

2. Then run this command:
    ```bash
    $ sudo udevadm control --reload-rules
    ```
3. Connect the device to the host PC using a USB cable.
4. A notification from Chrome will appear with an URL of the IDE. If not, the
   address for the IDE is https://intel.github.io/zephyrjs-ide/.
5. Click on the notification to open IDE in the browser.
6. Click on connect button and grant an access to the device.
7. Try uploading JS code to the device.

* **On Windows 8.1 and above:**

    The landing page detection is disabled on Windows on Chrome so you don't see a
    notification in the upper right corner when the device is connected to the host,
    but the WebUSB will continue to work. Visit the [IDE](https://intel.github.io/zephyrjs-ide/)
    site and click on connect.

Commands
--------

To get a full list of commands, run the help command.

```
acm> help
Welcome to the ZJS 'A Shell' interface!

Command list:
    help           Display this help information
    eval           Evaluate JavaScript in real time
    load   FILE    Save the input text into a file
    run    FILE    Run the JavaScript program in the file
    parse  FILE    Check if the JS syntax is correct
    stop           Stop current JavaScript execution
    ls             List all files
    cat    FILE    Print the contents of a file
    du     FILE    Estimate file space usage
    rm     FILE    Remove file or directory
    mv     F1 F2   Move a file F1 to destination F2
    clear          Clear the terminal screen
    erase          Erase flash file system and reboot
    boot   FILE    Set the file that should run at boot
    reboot         Reboot the device
```

### eval

`eval`

This command will set the device in a continuous evaluation mode.
The shell prompt will change to 'js>'

```
acm> eval
Ready to evaluate JavaScript.
        Ctrl+D to return to shell.
js>
```

From that point on it will evaluate each line of JavaScript independently
so you can run commands, create functions (as far as they fit in one line)
and get results.

### load

`load <filename>`

Sets the device ready to get raw text input.
The text will be saved to the FAT File system.

Example of raw text input:
```
acm> load test.js

Saving to 'test.js'
Ready for JavaScript.
        Ctrl+Z to finish transfer.
        Ctrl+X to cancel.
RAW>
```

### run

`run <filename>`

The program will be loaded from disk into memory, parsed and run.

In case of an error while parsing it will stop parsing and output
"Failed parsing JS".

### parse

`parse <filename>`

Parses the given file to check for errors without running it.

### stop

`stop`

Stops execution of any running JS script.

### ls

`ls`

List contents of current root folder.

### cat

`cat <filename>`

Print the contents of the given file to the terminal.

### du

`du <filename>`

Estimates the file usage of the given file.

### rm

`rm <filename>`

Deletes the specified file.

### mv

`mv <filename1> <filename2>`

Renames filename1 to filename2.

### clear

`clear`

Clears the terminal screen.

### erase

`erase`

Erase flash file system.

Note: this only works on the Arduino 101 and it will automatically rebooting the
board after erase is finished.

### boot

`boot <filename>`

Sets the specified file as a script to run automatically each time the device
boots.

### reboot

`reboot`

Reboots the device.

Connect using serial console
----------------------------
If you want to interact with the ashell using serial console instead of
browser IDE, use:
```bash
$ make ashell
```

* Then use the following command to connect to the ashell from a terminal:
  ```bash
  $ screen /dev/ttyACM0 115200
  ```

Note: It will take about 30 seconds for it to be up and running after you boot.
Until then you will see screen terminate immediately.  If you see this, just
try again in a few seconds.

Problems and known issues
========================

ZJS will only execute timeouts or events on the first run. Sometimes there is
a duplicated character written to the ACM.

LED2 on Arduino 101 is not available in ashell mode because the GPIO it is tied
to is being used for SPI to talk to the flash filesystem instead.

If you are using BLE module, by default BLE will be enabled but it cannot be
turned off once turned on, currently Zephyr doesn't support disabling BLE.  So
If you subsribe for BLE "stateChange" events, and/or want to register BLE GATT
services,  it will only work for the first time you run the app, and you will
need to reboot the board in order for it to work the second time.
first time you run in the IDE, after that, you'll have to
