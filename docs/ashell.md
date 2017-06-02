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

* Start Google Chrome 58 or later on the host PC.

* **On Ubuntu and Fedora:**
1. Create udev rules to allow Chrome to open the WebUSB enabled device and
   also prevent ModemManager interfering with that device by adding the following
   lines in /etc/udev/rules.d/99-arduino-101.rules

>     SUBSYSTEM=="tty", ENV{ID_VENDOR_ID}=="8086", ENV{ID_MODEL_ID}=="f8a1", MODE="0666", ENV{ID_MM_DEVICE_IGNORE}="1", ENV{ID_MM_CANDIDATE}="0"
>     SUBSYSTEM=="usb", ATTR{idVendor}=="8086", ATTR{idProduct}=="f8a1", MODE="0666" ENV{ID_MM_DEVICE_IGNORE}="1"

2. Then run this command:
    ```bash
    $ sudo udevadm control --reload-rules
    ```
3. Connect the device to the host PC using a USB cable.
4. A notification from Chrome will appear with an URL of the IDE. If not, the
   address for the IDE is https://01.org/iot-ide.
5. Click on the notification to open IDE in the browser.
6. Click on connect button and grant an access to the device.
7. Try uploading JS code to the device.

* **On Windows 8.1 and above:**

  WebUSB compatible device is not appearing on Windows by default for some reason,
  so try installing WinUSB driver with the Zadig utility.

  Here are the steps for installing the WinUSB driver with the Zadig utility:

1. Download the [Zadig](http://zadig.akeo.ie/downloads/zadig-2.3.exe) utility.
2. Connect WebUSB device to host
3. Start Zadig
4. In Zadig app under "Options" check the "List All Devices" option
5. In the drop down box select WebUSB (Interface 1)
6. Select the WinUSB driver
7. Press the "Install Driver" or "Reinstall Driver" button to install the WinUSB driver
8. After installation is done, select WebUSB (Interface 2) from the drop down box
   and install the WinUSB driver same as above.
9. Disconnect and reconnect the WebUSB device.
10. The landing page detection is disabled on Windows on Chrome so you don't see a
    notification in the upper right corner when the device is connected to the host,
    but the WebUSB will continue to work. Visit the [IDE](https://01.org/iot-ide)
    site and click on connect.

Commands
--------

To get a full list of commands, run the help command.

```
acm> help
'A Shell' bash

Commands list:
    help        This help
    eval        Evaluate JavaScript in real time
     run FILE   Runs the JavaScript program in the file
    stop        Stops current JavaScript execution
 bootcfg FILE   Set the file that should run at boot
      ls        List all files
     cat FILE   Print the file contents of a file
      rm FILE   Remove file or directory
      mv F1 F2  Move a file F1 to destination F2
   clear        Clear the terminal screen
    boot FILE   Set the file that should run at boot
  reboot        Reboots the device
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

Example of raw text input:
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
`set transfer raw`

Will be plain text that contains the code. No CRC or error checking.
After the transmission is finished you can parse or run the code.

2. Intel Hex
`set transfer ihex`
Basic CRC, hexadecimal data with data sections and regions.
It might be that the code is divided in sections and you will only update a section of the memory.
This will be used for the future integration to support JS Snapshots.

## Data transaction example using IHEX

The motivation of the IHEX use is to be able to upload binary or text
files and have some basic support for CRC.

```
acm> set transfer ihex
HEX> load
```

Then paste ihex data. For example:
```bash
:2000000066756E6374696F6E2074657374696E67286E756D62657229207B0A097265747514
:20002000726E206E756D6265722A323B0A7D3B0A7072696E742822486920776F726C642044
:0F004000222B74657374696E6728313829293B48
:00000001FF
```

Move the temporary file to a destination file:
```
HEX> mv temp.dat test.js
HEX> run test.js
```

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
