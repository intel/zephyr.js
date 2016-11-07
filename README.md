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
Copy these two files into your /etc/udev/rules.d directory:

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
#### ARC support image
Now you're ready to build the support image for the ARC core with this command:

```bash
$ make arc
```

Then connect the Arduino 101 to your host with a USB A/B cable. Press the
Master Reset button on the Arduino 101 and after a few seconds type:

```bash
$ make dfu-arc
```

This will flash the ARC image to the device using the dfu-util program.
(Note, this is not running the JavaScript engine and application yet, that will
occur on the x86 side below.)

If you get a permission error, make sure you followed the 'Join the plugdev
group' instructions above for this user. You shoudn't need to run this command
with `sudo`.

(The ARC image doesn't change very often so you won't need to do this again
unless you pull updates to the source tree, and usually not even then.)

#### x86 application image
Next, build the x86 image which includes the JerryScript engine and the
ZJS runtime support, along with your JavaScript application:

```bash
$ make JS=samples/TrafficLight.js
```

The JS= argument lets you provide the path to your application. The TrafficLight
sample is a good first choice because you don't need to wire up any additional
hardware. It just blinks onboard LEDs on your Arduino 101. Also, for many of
the samples you will want to hook up the serial console (see below), but for
this one it's not really needed.

Finally, you're ready to install and run your application. If you just flashed
the ARC image as above, your Arduino 101 will already be waiting for more DFU
commands. If not, press the Master Reset button, and either way do:

```bash
$ make dfu
```

After this flashing completes successfully, reboot the device with the Master
Reset button to start the application. After a few seconds the onboard LEDs
should start cycling.

You have built and run your first ZJS application!

If you want to make changes to the application, or run a different .js sample,
you just need to repeat the last two steps with the desired JavaScript filename.

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
By default, Arduino 101 comes with a **144K** X86 partition, but we're able to
pretty safely increase it to **256K**. You should only use the `dfu-util`
method of flashing (not JTAG) after you do this.

The easiest way to do this is to use a flashpack.zip file that I can provide
you (geoff@linux.intel.com). I need to make sure of the license details before
I add it to the repo.

The ZJS_PARTITION environment variable, set automatically by zjs-env.sh, is
what controls whether the build targets 144KB or 256KB. If you use
the wrong one, the ARC side won't come up because we'll be attempting
to start it from the wrong place on the flash. (Note: both the ARC and X86
images have to be rebuilt and reflashed when you switch partition sizes.)

If you are using ZJS with an Arduino 101 that you have converted to a 256KB X86
partition (see below), you should run this command when you set up your shell:
```bash
$ source zjs-env.sh 256
```

## Building system images
The ZJS project uses a top-level Makefile to control the building of code from
he project itself as well as the JerryScript and Zephyr projects it depends on.

To see the available make commands, type:

```bash
$ make help
```

## Build the ARC image
On Arduino 101, there are two embedded microcontrollers, an X86 and an ARC one.
If you only need the x86 side, you can disable ARC with CONFIG_ARC_INIT=n in
the Zephyr prj.conf. Otherwise, you need a working image running on it.

For the best flexibility in using ZJS, you should build and install our
project's ARC image. Some of the APIs you can call from JavaScript won't work
until you have this server image running there (for now this is just AIO API).
This is because only the ARC MCU can perform certain functions, like converting
analog inputs to digital values.

We don't change the ARC engine code very often, so you only need to update this
occasionally. To build and install the ARC image, do this:

```bash
$ make arc
$ make dfu-arc
```

(As always with DFU, you will need to reset the device a few seconds before you
attempt the DFU command, or it will fail.)

## Build the *Hello World* sample
```bash
$ make JS=samples/HelloWorld.js
```

This will build the system image, resulting in `outdir/arduino_101/zephyr.bin`
as the final output. To flash this to your device with dfu-util, first press
the Master Reset button on your Arduino 101, and about three seconds later type:

```bash
$ make dfu
```

There is a window of about five seconds where the DFU server is available,
starting a second or two after the device resets.

Now the x86 image on your device has been updated. Press the Master Reset button
one more time to boot your new image.

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
