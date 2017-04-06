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

Compilation
------------

This command will enable the shell compilation for use with the IDE:
`make ide`

To use the shell via the command line use:
`make ashell`

Dev mode will append all the functionality from ZJS, so it might not fit in ROM.

Connect
-------

WebUSB support was added to the ashell to allow the user to upload JS code
directly from the browser IDE to Zephyr.js device for execution. Follow the
below instructions to connect to the device from the browser IDE directly.

1. Start Google Chrome 54 or later on the host PC.
2. Enable "WebUSB" support and "Experimental Web Platform Features"
   flags in chrome://flags/.
3. On Ubuntu:
  * Create a new udev rule to allow Chrome to open a USB device by adding
    the following line in /etc/udev/rules.d/99-arduino-101.rules

     `SUBSYSTEM=="usb", ATTR{idVendor}=="8086", MODE="0664", GROUP="plugdev"`

     Then run this command:
     ```bash
     $ sudo udevadm control --reload-rules
     ```
  * Disable ModemManager to stop interfering when the browser accessing the device.

     ```bash
     $ sudo service modemmanager stop
     ```
    If you get the message "Unit modemmanager.service not loaded." Try this instead...

    ```bash
    $ sudo service ModemManager stop
    ```

4. On Windows:
  * WebUSB compatible device is not appearing with the official WinUSB driver on
    Windows for some reason, so try installing different version of WinUSB driver
    with the Zadig utility. Also, note that the landing page detection is disabled
    on Windows on Chrome startup so you don't see a notification when the device is
    connected to the host, but the WebUSB will continue to work.

5. Connect the device to the host PC using a USB cable.
6. A notification from Chrome will appear with an URL of the IDE.
7. Click on the notification to open IDE in the browser.
8. Click on connect button and grant an access to the device.
9. Try uploading JS code to the device.

Commands
--------

To get a full list of commands, run the help command.

```
acm> help
'A Shell' bash

Commands list:
    help This help
    eval Evaluate JavaScript in real time
   clear Clear the terminal screen
    load [FILE] Saves the input text into a file
     run [FILE] Runs the JavaScript program in the file
   parse [FILE] Check if the JS syntax is correct
    stop Stops current JavaScript execution
      ls [FILE] List directory contents or file stat
     cat [FILE] Print the file contents of a file
      du [FILE] Estimate file space usage
      rm [FILE] Remove file or directory
      mv [SOURCE] [DEST] Move a file to destination
    test Runs your current test
   error Prints an error using JerryScript
    ping Prints '[PONG]' to check that we are alive
      at OK used by the driver when initializing
     set Sets the input mode for 'load' accept data
        transfer raw
        transfer ihex
     get Get states on the shell
  reboot Reboots the device
```

### Eval

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

### cat

`cat <filename>`

Output the text from file to the terminal

### run

`run <filename>`

The program will be loaded from disk into memory, parsed
and run.

In case of an error while parsing it will stop parsing and output
"Failed parsing JS"

### ls

`ls`

List contents of current root folder

### rm

`rm <filename>`

Remove file

### Load

`load <filename>`

Sets the device ready to get raw or ihex text input.
The text will be recorded in the FAT File system.

Example of raw text input
```
acm> load test.js

Saving to 'test.js'
Ready for JavaScript.
        Ctrl+Z to finish transfer.
        Ctrl+X to cancel.
RAW>
```

### Data transfer

Set the mode to accept data when the 'load' transmission starts

1. Raw data
Will be plain text that contains the code. No CRC or error checking.
After the transmission is finished you can parse or run the code.

2. Intel Hex
Basic CRC, hexadecimal data with data sections and regions.
It might be that the code is divided in sections and you will only update a section of the memory.
This will be used for the future integration to support JS Snapshots.

```
set transfer ihex
set transfer raw
```

## Data transaction example using IHEX

The motivation of the IHEX use is to be able to upload binary or text
files and have some basic support for CRC.

```
acm> set transfer ihex
HEX> load
```
<Send ihex data here>
Example:
```
:2000000066756E6374696F6E2074657374696E67286E756D62657229207B0A097265747514
:20002000726E206E756D6265722A323B0A7D3B0A7072696E742822486920776F726C642044
:0F004000222B74657374696E6728313829293B48
:00000001FF
```
Move the temporary file to a destination file
```
HEX> mv temp.dat test.js
HEX> run test.js
```

Connect using serial console
----------------------------
If you want to interact with the ashell using serial console instead of
browser IDE, use: `make ashell`

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
