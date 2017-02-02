# JavaScript\* Runtime for Zephyr&trade; OS

The JavaScript\* Runtime for the Zephyr&trade; OS project (ZJS for short) provides an IoT
web runtime environment with JavaScript APIs for the Zephyr operating system,
based on the JerryScript engine. It is intended for systems with little memory
where Node.js with V8 is too big.

This code requires a local copy of JerryScript and Zephyr OS source.  We
will upstream patches to those projects as appropriate, but this repo is for
everything else.

## Getting Started

This section will walk you through building and running your first ZJS
application on Arduino 101\*.

### Prerequisites
* [Arduino 101 board](https://www.arduino.cc/en/Main/ArduinoBoard101)
* Ubuntu\* 16.04 host; adapt as necessary for other platforms.
* If you're behind a proxy, go through all the [usual pain]
(https://github.com/01org/zephyr.js/wiki/Proxy) to get ssh working to
github.com and http working to zephyrproject.org.

### Initial Setup
***Windows and OSX users***: These instructions are for Ubuntu 16.04. Be sure to
also consult Zephyr Project's [Getting Started](https://www.zephyrproject.org/doc/getting_started/getting_started.html)
documentation for [Windows](https://www.zephyrproject.org/doc/getting_started/installation_win.html)
or [OSX](https://www.zephyrproject.org/doc/getting_started/installation_mac.html).

#### Install dependencies
First, install these packages that you will need beyond those installed by
default with Ubuntu:

```bash
$ sudo apt-get update
$ sudo apt-get install cmake dfu-util git screen uglifyjs
```

#### Clone the ZJS repo
Next, clone this git repo:
```bash
$ git clone http://github.com/01org/zephyr.js.git
```

#### Check out the stable tag
Next, unless you know you have other plans, check out the `stable` tag. This is
currently based on the stable Zephyr 1.5.0 release and should be the best
working version. (By this we mean it had the highest pass rate by our QA folks.)

```bash
$ git checkout stable
```

If you do nothing and remain on master, you will be looking at the very latest
changes which may have regressions or instability. Instead, you could check out
the `latest` tag which will still be pretty recent but at least sanity-checked
by QA and known to basically work. You can read the `version` file in the root
of the source tree to see what version you're on. If you're on the master
development branch it will just say 'devel'.

#### Install the Zephyr SDK
Download the [latest Zephyr SDK] (https://www.zephyrproject.org/downloads/tools), then:
```bash
$ chmod +x /path/to/zephyr-sdk-<VERSION>-i686-setup.run
$ sudo /path/to/zephyr-sdk-<VERSION>-i686-setup.run
```

Follow the prompts, but the defaults should be fine.

#### Set up Zephyr SDK environment variables
Add the following two lines to your `~/.bashrc`. If you installed your Zephyr SDK
elsewhere, adjust as needed.
```bash
export ZEPHYR_GCC_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk
```

Then source the .bashrc :
```bash
$ source ~/.bashrc
```

#### Join the plugdev group
Add your user to the plugdev group with this command:

```bash
$ sudo usermod -a -G plugdev USERNAME
```

#### Add udev rules
Copy these two files into your /etc/udev/rules.d directory (/etc/udev.rules for Ubuntu 14.04):

* [99-openocd.rules](https://raw.githubusercontent.com/arduino/OpenOCD/master/contrib/99-openocd.rules) (from this [Arduino github project](https://github.com/arduino/OpenOCD/blob/master/contrib/99-openocd.rules))
* [99-dfu.rules](https://raw.githubusercontent.com/01org/CODK-A-ARC/master/bin/99-dfu.rules) (from this [01org github project](https://github.com/01org/CODK-A-ARC/blob/master/bin/99-dfu.rules))

Then run this command:

```bash
$ sudo udevadm control --reload-rules
```

This should cause your `/dev/tty*` entries to have the plugdev group, which will
let you use them without root privileges. Otherwise, you will have to run some
of the following commands with `sudo`.

### Shell setup
Whenever you open a new terminal to work with this repo, you need to set up
environment variables.

#### Set up ZJS environment variables
First, the ZJS variables:

```bash
$ cd zephyr.js
$ source zjs-env.sh
```

#### Get source dependencies
Next, this command will check out additional git repos into the deps/
subdirectory, if you haven't done so before:

```bash
$ make update
```

(If this is the first time you've run this, will see an error.)

#### Set up Zephyr OS environment variables
As the previous command will complain, you need to set up some Zephyr OS
environment variables, too. Here's the right way to do that:

```bash
$ source deps/zephyr/zephyr-env.sh
```

### Build and Flash
#### x86 application image and ARC support image
Now you're ready to build the x86 and ARC images. The x86 image
includes the JerryScript engine and the ZJS runtime support, along with
your JavaScript application, and the ARC support image acts as a helper
library that channels some of the data needed from the ARC processor to the
x86 processor.

**Note**, you'll need to build both the x86 and ARC images
with the same JS file so the required sub-modules are enabled on both images.

You can build both with a single command:

```bash
$ make JS=samples/TrafficLight.js all
```

The JS= argument lets you provide the path to your application. The TrafficLight
sample is a good first choice because you don't need to wire up any additional
hardware. It just blinks onboard LEDs on your Arduino 101. Also, for many of
the samples you will want to hook up the serial console (see below), but for
this one it's not really needed.

Then connect the Arduino 101 to your host with a USB A/B cable. Press the
Master Reset button on the Arduino 101 and after a few seconds type:

```bash
$ make dfu-all
```

This will flash both the images to the device using the dfu-util program.

If you get a permission error, make sure you followed the 'Join the plugdev
group' instructions above for this user. You shouldn't need to run this command
with `sudo`.

After this flashing completes successfully, reboot the device with the Master
Reset button to start the application. After a few seconds the onboard LEDs
should start cycling.

You have built and run your first ZJS application!

If you want to make changes to the application, or run a different .js sample,
you just need to repeat the steps the desired JavaScript filename.

#### x86 application image only
If you want to build the x86 and ARC images separately, or if you just want to
build another .js sample that uses the same sub-modules, then you can build only
the x86 image with:

```bash
$ make JS=path/to/filename.js
$ make dfu
```

#### ARC support image only
To build and flash only the ARC support image:

```bash
$ make JS=path/to/filename.js arc
$ make dfu-arc
```

If you run a different .js sample that uses different sub-modules, for example,
from AIO.js to I2C.js, then you'll need to repeat the last two steps to flash
both images.

If you forget to update the ARC image when your module requirements have changed,
you'll see an error in the serial console (setup steps below) similar to this:

```bash
ipm_console0: 'unsupported ipm message id: 1, check ARC modules'
```

### Next steps

#### Set up serial console

Without the serial console set up, you won't be able to see error messages and
other output from your ZJS application. To hook up the serial console, you need
a USB to TTL Serial Cable, such as the TTL-232R-3V3. On that particular cable,
you wire the black wire to ground on the Arduino 101 board, the orange wire to GPIO pin 0
(RX), and the yellow wire to GPIO pin 1 (TX). The other three are unused.

When you plug this in, the device should show up as something such as
`/dev/ttyUSB0`. You can then use the screen command to connect to the device
with a command such as this:

```bash
$ watch screen /dev/ttyUSB0 115200
```

The `watch` utility will restart screen when you disconnect and reconnect your
Arduino 101, so you shouldn't miss anything. You can leave a dedicated terminal
running to watch the output.

In `screen`, you can scroll back the output with Ctrl-A, Esc, followed by PgUp/PgDn.
Then Esc again to get back to the latest output (out of "Copy Mode").

#### Debugging

You can use the commands `make debug` and `make gdb` in two separate terminals
to connect to the device with a debugger. Then you can set breakpoints such as
`b main` and `run` to start debugging as usual with gdb.

#### Additional details

See below for a few more tips, such as increasing the space available for your
application on the Arduino 101, or how to use ZJS with the FRDM-K64F.

## Contributing

If you want to contribute code to the ZJS project, first you need to fork the
project. The next step is to send a pull request (PR) for review to the ZJS
repository. The PR will be reviewed by the project team members. You need at
least two plus-ones (+1) , "Look Good To Me (LGTM)" or other positive signals
for the project members. Once you have gained the required signals the project
maintainers will merge the PR.

## File Descriptions
* `zjs-env.sh` - Source this file to set environment variables and path to be
able to use tools from ```scripts/``` anywhere.
* `prj.conf` - The main configuration file for a Zephyr application; overrides
settings from a defconfig file in the Zephyr tree. In the ZJS builds, we
assemble the prj.conf file at build time from other fragments.
* `prj.mdef` - Another configuration file for a Zephyr application; we use it to
configure the heap size available to the ZJS API.

## Subdirectories
- `arc/` - Contains sensor subsystem code for ARC side of the Arduino 101.
- `deps/` - Contains dependency repos and scripts for working with them.
- `docs/` - Documentation in Markdown format (use API.md as index).
- `outdir/` - Directory generated by build, can be safely removed.
- `samples/` - Sample JavaScript files that can be built with make JS=<path>.
- `scripts/` - Subdirectory containing tools useful during development.
- `src/` - JS API bindings for JerryScript written directly on top of Zephyr.
- `tests/` - JavaScript unit tests (incomplete).

## Getting more space on your Arduino 101
Arduino 101 comes with a **144K** X86 partition, but we're able to use more
space by telling Zephyr there is more space and then splicing the images we
flash to the device. You can control this with the SIZE= flag to make. So if
you want to allocated 256KB for x86, use SIZE=256.

You can also just build without it until you see a message like this:
```
lfiamcu/5.2.1/real-ld: region `ROM' overflowed by 53728 bytes
```

That implies you need an extra 53K of space, so you could try passing SIZE=200.
If it's the ARC image that needs more space, you should decrease the SIZE you're
passing instead.

**NOTE**: Earlier, we would physically repartition the device and install a new
bootloader that knew about it. This is no longer necessary, so if you have such
a device you should restore it to factory condition with the 256-to-144
flashpack.

## Building system images
The ZJS project uses a top-level Makefile to control the building of code from
he project itself as well as the JerryScript and Zephyr projects it depends on.

To see the available make commands, type:

```bash
$ make help
```

On Arduino 101, there are two embedded microcontrollers, an X86 and an ARC one.
If you only need the x86 side, you can disable ARC with CONFIG_ARC_INIT=n in
the Zephyr prj.conf. Otherwise, you need a working image running on it.

## Build the *Hello World* sample
```bash
$ make JS=samples/HelloWorld.js
```

This will build both an X86 and an ARC image, resulting in
`outdir/arduino_101/zephyr.bin` and `arc/outdir/arduino_101_sss/zephyr.bin`
as the final output. Then adjusted versions are created with a `.dfu` suffix.
To flash them to your device with dfu-util, first press the Master Reset button
on your Arduino 101, and about three seconds later type:

```bash
$ make dfu
```

There is a window of about five seconds where the DFU server is available,
starting a second or two after the device resets.

Now both images on your device have been updated. Press the Master Reset button
one more time to boot your new images.

## Build other samples
The other samples may require some hardware to be set up and connected; read
the top of each JS file, but then simply pass in the path to the JS file to make
as with `HelloWorld.js` above.

## JS Minifier

To save space it is recommended to use a minifier. In `convert.sh`, the script
used to encode your JS into a source file, we use `uglifyjs`. If you didn't
install this earlier, you can do so with the command:
```bash
sudo apt-get install node-uglify
```

## FRDM-K64F Platform

See the
[Zephyr Project Wiki] (https://wiki.zephyrproject.org/view/NXP_FRDM-K64F)
for general information about running Zephyr OS on the FRDM-K64F.

The instructions below assume Ubuntu 14.04 on the host PC.

Connect a micro-USB cable from the device to your PC.

If you hit the Reset switch and wait about five seconds, you should be able to
start up a serial console. Either:

```bash
$ screen /dev/ttyACM0 115200
```
or
```bash
$ minicom -D /dev/ttyACM0
```

(I typically had to try either command several times before it
would work.) The benefit of minicom is it will keep running even if you unplug
the cable and then plug it back in later.

Check your dmesg output or watch your /dev directory to know what device it
shows up as.

Then, follow [these instructions] (https://developer.mbed.org/handbook/Firmware-FRDM-K64F) to update your firmware.

Next, you can try to build ZJS for the platform:
```bash
$ make BOARD=frdm_k64f JS=samples/HelloWorld.js
$ cp outdir/frdm_k64f/zephyr.bin /media/<USERNAME>/MBED/
```

After you copy the new `.bin` file to that directory, the device will reboot,
blink an LED quickly as it writes the image, and then you should see
the device reconnect as a USB storage device to your PC. Then you can press the
Reset button to run the Zephyr image. You should see "Hello, ZJS world!" output
on the serial console in less than a second.

If something doesn't work, you may want to establish that you're able to
upload the K64F [hello world application] (https://developer.mbed.org/platforms/FRDM-K64F/#flash-a-project-binary).

Then, you could try the Zephyr OS `hello_world` sample to narrow down the
problem:
```bash
$ cd deps/zephyr/samples/hello_world/nanokernel
$ make pristine && make BOARD=frdm_k64f
$ cp outdir/frdm_k64f/zephyr.bin /media/<USERNAME>/MBED/
```

Using the same procedure as above, once you hit Reset you should see
"Hello World!" within a second on your serial console.

Zephyr is a trademark of the Linux Foundation. *Other names and brands may be claimed as the property of others.

## Building and running on Linux/Mac OSX
In addition to Zephyr there is a "linux" target which does not use Zephyr at all
and instead uses the host OS. This can be build on Linux or Mac OSX using the
command:

```bash
make linux
```

The executable will be outputted to `outdir/linux/<variant>/jslinux`. Where
`<variant>` is either `debug` or `release`. This is specified the same as the
Zephyr target by passing in `VARIANT=` when running `make`. The default is
release.

What makes the linux target convenient is that a JS script does not have to be
bundled with the final executable. By default `samples/HelloWorld.js` will be
bundled but you can always just pass in a script on the command line when running
jslinux e.g.:

```bash
./outdir/linux/release/jslinux samples/Timers.js
```

If a script is passed in on the command line it will take the priority over any
script bundled with the executable (using `JS=`).

By default jslinux will run forever (as Zephyr does) but if this is not desired,
there are two flags which can be used to cause jslinux to exit under certain
conditions. The first is the `--autoexit` flag. If this flag is used, jslinux
will continually check to see if there are any pending events, like timers,
callbacks or service functions. If there have no been any events processed on the
last and most current loop cycle it will exit. The second flag (`-t`) will cause
jslinux to exit after a specified timeout in milliseconds. This flag can be used like:

```bash
./jslinux -t <ms>
```

The `--autoexit` and `-t <ms>` flags can be used together, which will cause
jslinux to exit on whichever condition is met first.

It should be noted that the Linux target has only very partial support to hardware
compared to Zephyr. This target runs the core code, but most modules do not run
on it, specifically the hardware modules (AIO, I2C, GPIO etc.). There are some
modules which can be used though like Events, Promises, Performance and OCF. This
list may grow if other modules are ported to the Linux target.

## Supported modules (Linux vs Zephyr)
There is only partial support for modules on Linux compared to Zephyr. Any hardware
specific module (I2C, UART, GPIO, ADC etc.) is not supported on Linux. Trying
to run a Zephyr specific module on Linux will result in the JavaScript not running
successfully. Below is a complete table of modules and target support.

| Module    | Linux                    | Zephyr                   |
| :---:     | :---:                    | :---:                    |
|  ADC      | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
|  PWM      | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
|  GPIO     | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
|  I2C      | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
|  BLE      | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
|  UART     | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
| Sensor    | <ul><li>- [ ] </li></ul> | <ul><li>- [x] </li></ul> |
| Buffer    | <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |
| Console   | <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |
| Event     | <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |
| OCF       | <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |
|Performance| <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |
| Timers    | <ul><li>- [x] </li></ul> | <ul><li>- [x] </li></ul> |

|
