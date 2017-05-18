# JavaScript\* Runtime for Zephyr&trade; OS

The JavaScript\* Runtime for the Zephyr&trade; OS project (ZJS for short) provides an IoT
web runtime environment with JavaScript APIs for the Zephyr operating system,
based on the JerryScript engine. It is intended for systems with little memory
where Node.js with V8 is too big.

This code requires a local copy of JerryScript and Zephyr OS source. We
will upstream patches to those projects as appropriate, but this repo is for
everything else.

### Index
* [Getting Started](#getting-started)
* [Contributing](#contributing)
* [Repo Organization](#repo-organization)
* [Increase space on A101](#getting-more-space-on-your-arduino-101)
* [JS Minifier](#js-minifier)
* [FRDM-K64F Platform](#frdm-k64f-platform)
* [Building and running on Linux](#building-and-running-on-linux)
* [Building and running on Mac OSX](#building-and-running-on-mac-osx)
* [Supported modules on Linux and Zephyr](#supported-modules-on-linux-and-zephyr)
* [Networking with QEMU](#networking-with-qemu)
* [Networking with BLE](#networking-with-ble)

## Getting Started

This section will walk you through building and running your first ZJS
application on Arduino 101\*.

### Prerequisites
* [Arduino 101 board](https://www.arduino.cc/en/Main/ArduinoBoard101)
* Ubuntu\* 16.04 host; adapt as necessary for other platforms.
* If you're behind a proxy, go through all the [usual pain](https://github.com/01org/zephyr.js/wiki/Proxy) to get ssh working to
github.com and http working to zephyrproject.org.
* If you are a board pre-flashed with IDE, you can follow the ashell/IDE [guide](./docs/ashell.md) on how to build
and run your app using Web IDE.

### Initial Setup
***Windows and OSX users***: These instructions are for Ubuntu 16.04. Be sure to
also consult Zephyr Project's [Getting Started](https://www.zephyrproject.org/doc/getting_started/getting_started.html)
documentation for [Windows](https://www.zephyrproject.org/doc/getting_started/installation_win.html)
or [OSX](https://www.zephyrproject.org/doc/getting_started/installation_mac.html).

#### Install dependencies
First, install these packages that you will need beyond those installed by
default with Ubuntu:

```bash
sudo apt-get update
sudo apt-get install cmake dfu-util git python-yaml screen uglifyjs
```

Note: python-yaml is a recent requirement for the frdm-k64f build due to some
change in Zephyr, so it could be left out currently if you don't use k64f.

#### Clone the ZJS repo
Next, clone this git repo:
```bash
git clone http://github.com/01org/zephyr.js.git
```

#### Check out the desired version
If you want to use a stable release version, the latest is 0.3:

```bash
git checkout v0.3
```

If you do nothing and remain on master, you will be looking at the very latest
changes which may have regressions or instability. You can read the `version`
file in the root of the source tree to see what version you're on. If you're on
the master development branch it will just say 'devel'.

#### Install the Zephyr SDK
Download the [latest Zephyr SDK](https://www.zephyrproject.org/downloads), then:
```bash
chmod +x /path/to/zephyr-sdk-<VERSION>-setup.run
sudo /path/to/zephyr-sdk-<VERSION>-setup.run
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
source ~/.bashrc
```

#### Join the plugdev group
Add your user to the plugdev group with this command:

```bash
sudo usermod -a -G plugdev USERNAME
```

#### Add udev rules
Copy these two files into your /etc/udev/rules.d directory (/etc/udev.rules for Ubuntu 14.04):

* [60-openocd.rules](https://raw.githubusercontent.com/arduino/OpenOCD/master/contrib/60-openocd.rules) (from this [Arduino github project](https://github.com/arduino/OpenOCD/blob/master/contrib/60-openocd.rules))
* [99-dfu.rules](https://raw.githubusercontent.com/01org/CODK-A-ARC/master/bin/99-dfu.rules) (from this [01org github project](https://github.com/01org/CODK-A-ARC/blob/master/bin/99-dfu.rules))

Then run this command:

```bash
sudo udevadm control --reload-rules
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
cd zephyr.js
source zjs-env.sh
```

#### Get source dependencies
Next, this command will check out additional git repos into the deps/
subdirectory, if you haven't done so before:

```bash
make update
```

(If this is the first time you've run this, will see an error.)

#### Set up Zephyr OS environment variables
As the previous command will complain, you need to set up some Zephyr OS
environment variables, too. Here's the right way to do that:

```bash
source deps/zephyr/zephyr-env.sh
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
make JS=samples/TrafficLight.js
```

The JS= argument lets you provide the path to your application. The TrafficLight
sample is a good first choice because you don't need to wire up any additional
hardware. It just blinks onboard LEDs on your Arduino 101. Also, for many of
the samples you will want to hook up the serial console (see below), but for
this one it's not really needed.

Then connect the Arduino 101 to your host with a USB A/B cable. Press the
Master Reset button on the Arduino 101 and after a few seconds type:

```bash
make dfu
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

### Build with Web-based IDE
You can also build your app using our Web-based IDE, which allows you to write JS apps on the fly
in our online editor and upload them directly to the Arduino 101 board through WebUSB.  To use the
Web IDE, follow the [instructions](./docs/ashell.md) here to build and connect tto the IDE.

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
watch screen /dev/ttyUSB0 115200
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

### Travis and local sanity checking

We run a series of tests on each pull request and merged commit using Travis.
This relies on a script in the repo called `trlite`. One easy way to run these
tests on your local $ZJS_BASE git tree is to use `make check` or
`make quickcheck` for a faster subset of the tests. These run with your code as
it stands in your tree. This will not catch a problem like you failing to
add a new file to your commit.

For a slightly safer sanity check, which might catch that kind of problem, you
can run `trlite` directly or `trlite linux` for the "quick subset". This will
clone a second copy of your git tree into a .trlite subdirectory, apply changes
that `git diff` knows about, and run the build tests. Another option `trlite -j`
will cause it to run four threads of tests to speed up execution; these will
use four directories named `.trlite[1-4]`. If there is a test failure, the
affected `.trlite*` trees are left in place so that you can investigate.

## Repo Organization

### File Descriptions
* `zjs-env.sh` - Source this file to set environment variables and path to be
able to use tools from ```scripts/``` anywhere.
* `prj.conf` - The main configuration file for a Zephyr application; overrides
settings from a defconfig file in the Zephyr tree. In the ZJS builds, we
assemble the prj.conf file at build time from other fragments.
* `prj.mdef` - Another configuration file for a Zephyr application; we use it to
configure the heap size available to the ZJS API.

### Subdirectories
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
flash to the device. You can control this with the ROM= flag to make. So if
you want to allocated 256KB for x86, use ROM=256.

You can also just build without it until you see a message like this:
```
lfiamcu/5.2.1/real-ld: region `ROM' overflowed by 53728 bytes
```

That implies you need an extra 53K of space, so you could try passing ROM=200.
If it's the ARC image that needs more space, you should decrease the ROM you're
passing instead.

**NOTE**: Earlier, we would physically repartition the device and install a new
bootloader that knew about it. This is no longer necessary, so if you have such
a device you should restore it to factory condition with the 256-to-144
flashpack.

You can also influence the amount of RAM allocated to the X86 side with a new
RAM= argument. Here the default is 55 but it can theoretically go as high as
79 if ARC was disabled; realistically up to maybe 75 or so depending on how
few modules you require in the ARC build.

The RAM and ROM sizes being used are now displayed at the top of the make
output when you do build for Arduino 101.

## JS Minifier

To save space it is recommended to use a minifier. In `convert.sh`, the script
used to encode your JS into a source file, we use `uglifyjs`. If you didn't
install this earlier, you can do so with the command:
```bash
sudo apt-get install node-uglify
```

## nRF52 Platform
This is an experimental ZJS platform and has not been tested. There should be
no expectation that any given sample/test/application will work at all on this
platform. The good news is that there have been ZJS networking samples run
on the nRF52 board with success therefore we mention it here so anyone can try
it out and contribute fixes to anything that does not work, potentially getting
it stable enough to adopt as a supported board in the future.

See the
[Zephyr Project page](https://www.zephyrproject.org/doc/boards/arm/nrf52840_pca10056/doc/nrf52840_pca10056.html)
for general information about running Zephyr OS on the nRF52.

Connecting to serial output is quite similar to the Arduino 101, except the
nRF52 will have an ACM port rather than USB. You can connect with minicom
by doing:
```bash
minicom -D /dev/ttyACM0
```

Building is the same as any other ZJS platform, just use `nrf52840_pca10056` as
the BOARD name:
```bash
make JS=samples/OcfServer.js BOARD=nrf52840_pca10056
```

You should now have a Zephyr binary in `outdir/nrf52840_pca10056/`. You can
copy it to the nRF52 board with a simple `cp` command:
```bash
cp outdir/nrf52840_pca10056/zephyr.bin /media/<user>/JLINK/
```
You should see the lights flashing on the nRF52 board. When it stops you can
reset the board and you should see your application output on /dev/ttyACM0

From here the device can be connected with BLE to a Linux machine as you do with
an Arduino 101.

## FRDM-K64F Platform

See the
[Zephyr Project Wiki](https://wiki.zephyrproject.org/view/NXP_FRDM-K64F)
for general information about running Zephyr OS on the FRDM-K64F.

The instructions below assume Ubuntu 14.04 on the host PC.

Connect a micro-USB cable from the device to your PC.

If you hit the Reset switch and wait about five seconds, you should be able to
start up a serial console. Either:

```bash
screen /dev/ttyACM0 115200
```
or
```bash
minicom -D /dev/ttyACM0
```

(I typically had to try either command several times before it
would work.) The benefit of minicom is it will keep running even if you unplug
the cable and then plug it back in later.

Check your dmesg output or watch your /dev directory to know what device it
shows up as.

Then, follow [these instructions](https://developer.mbed.org/handbook/Firmware-FRDM-K64F) to update your firmware.

Next, you can try to build ZJS for the platform:
```bash
make BOARD=frdm_k64f JS=samples/HelloWorld.js
cp outdir/frdm_k64f/zephyr.bin /media/<USERNAME>/MBED/
```

After you copy the new `.bin` file to that directory, the device will reboot,
blink an LED quickly as it writes the image, and then you should see
the device reconnect as a USB storage device to your PC. Then you can press the
Reset button to run the Zephyr image. You should see "Hello, ZJS world!" output
on the serial console in less than a second.

If something doesn't work, you may want to establish that you're able to
upload the K64F [hello world application](https://developer.mbed.org/platforms/FRDM-K64F/#flash-a-project-binary).

Then, you could try the Zephyr OS `hello_world` sample to narrow down the
problem:
```bash
cd deps/zephyr/samples/hello_world/
make pristine && make BOARD=frdm_k64f
cp outdir/frdm_k64f/zephyr.bin /media/<USERNAME>/MBED/
```

Using the same procedure as above, once you hit Reset you should see
"Hello World!" within a second on your serial console.

Zephyr is a trademark of the Linux Foundation. *Other names and brands may be claimed as the property of others.

## Building and running on Linux
In addition to Zephyr there is a "linux" target which does not use Zephyr at all
and instead uses the host OS. This can be built on Linux or Mac OSX using the
command:

```bash
make BOARD=linux
```

The executable will be outputted to `outdir/linux/<variant>/jslinux`. Where
`<variant>` is either `debug` or `release`. This is specified the same as the
Zephyr target by passing in `VARIANT=` when running `make`. The default is
release.

Note: To build on Mac OSX using BOARD=linux, see instructions in the next section.

What makes the linux target convenient is that a JS script does not have to be
bundled with the final executable. By default `samples/HelloWorld.js` will be
bundled but you can always just pass in a script on the command line when running
jslinux e.g.:

```bash
./outdir/linux/release/jslinux samples/Timers.js
```

If a script is passed in on the command line it will take the priority over any
script bundled with the executable (using `JS=`).

By default jslinux will exit when there are no pending events but if this is
not desired, there are two flags which can be used to cause jslinux to run
longer (or forever). The first is the `--noexit` flag. If this flag is used,
jslinux will run indefinitely. The second flag (`-t`) will cause jslinux to
run until a specified timeout (in milliseconds) is met.
This flag can be used like:

```bash
./jslinux -t <ms>
```

It should be noted that the Linux target has only very partial support to hardware
compared to Zephyr. This target runs the core code, but most modules do not run
on it, specifically the hardware modules (AIO, I2C, GPIO etc.). There are some
modules which can be used though like Events, Promises, Performance and OCF. This
list may grow if other modules are ported to the Linux target.

## Building and running on Mac OSX

Mac support is still limited at this point. As Zephyr does not provide the SDK/toolchain
to compile on Mac OSX natively, you will have to build your own or use a 3rd-party toolchain.
Currently the targets we support building on Mac are "linux", "qemu_x86", with limited
support for "arduino_101" and "frdm-k64f" boards. You'll need to have a recent version of
OSX and XCode command line tools from App store to get you started. Depending on your
system setup and target, you might have to perform additional steps, but if you run
into build issues, you should first make sure that you can build Zephyr native apps on Mac
using the toolchain you installed. Once you verify that it works, then our project should
also build and link correctly, but we'll try to update the document as we find these kinds
of issues. Currently we enable Travis CI to build the linux target only against the
latest OSX (10.12) and XCode Command Line Tools (provided by XCode SDK) for sanity check
purposes.

### Set up
The basic requirement for building Zephyr boards is that you'll need to install and
setup the correct cross-compiler toolchain on your Mac for the boards you are trying to
build. You need to install crosstool-ng, which allows you to build the x86 images
you can run on QEMU, and for Arduino 101, you need to install the ARC compiler,
which can be found by installing the Arduino IDE for Mac, this is used to build the ARC
support image.

Requirements:

* OSX: Sierra 10.12 or later
* XCode Command Line Tools: 8.1 or later (clang)
* Python: 2.7 or later
* Homebrew
* Crosstool-ng
* ARC cross-compiler (for building Arduino 101)
* Python-yaml

First, make sure you have Homebrew installed, instructions [here](https://brew.sh/), and then install the following brew packages:
```bash
brew install cmake libtool automake gcc
```

Next, you'll need to update to the latest OSX and install/upgrade to
the latest XCode Command Line Tools (we build Sierra 10.12 and XCode 8.3) from App store.

To install XCode Command Line Tools, open a terminal and type:
```bash
xcode-select --install
```

It should pop up a window to ask you to install the tools. You'll then set up the environment variables
```bash
cd zephyr.js
source zjs-env.sh
make update
source deps/zephyr/zephyr-env.sh
```

### Building Linux target
You can build the "linux" target on Mac OSX using BOARD=linux, follow instructions for "Building and running on Linux". This will create the jslinux ouput.

### Building QEMU and Arduino 101 targets
You can build QEMU with BOARD=qemu_x86 and Arduino 101 with BOARD=arduino_101, you'll need to install crosstool-ng and ARC compiler from Arduino IDE. (**Note** there's a linker issue currently with crosstool-ng when building Arduino 101 see [here](https://jira.zephyrproject.org/browse/ZEP-1994), but qemu_x86 should work)

Install crosstool-ng following the Zephyr instructions [here](https://www.zephyrproject.org/doc/getting_started/installation_mac.html)

After installing crosstool-ng, create and mount the image using our script [osxmountzephyr.sh](scripts/osxmountzephyr.sh):
```bash
osxmountzephyr.sh
```

Once you've created the image the first time, you can subsequently re-mount and un-mount the images with:
```bash
hdiutil mount CrossToolNG.sparseimage
diskutil umount force /Volumes/CrossToolNG
```

This will create an image mounted under /Volumes/CrossToolNG.  You can then configure crosstool-ng:
```bash
cd /Volumes/CrossToolNG
mkdir build
cd build
cp ${ZEPHYR_BASE}/scripts/cross_compiler/x86.config .config
```

(optional). You can customize an existing configuration yourself using the configuration menus:
```bash
ct-ng menuconfig
```

After you are done, edit the generated .config, and make sure you have these settings, or you will run into build errors later:
```bash
...
CT_LOCAL_TARBALLS_DIR="/Volumes/CrossToolNG/src"
# CT_SAVE_TARBALLS is not set
CT_WORK_DIR="${CT_TOP_DIR}/.build"
CT_PREFIX_DIR="/Volumes/CrossToolNG/x-tools/${CT_TARGET}"
CT_INSTALL_DIR="${CT_PREFIX_DIR}"
# Following options prevent link errors
CT_WANTS_STATIC_LINK=n
CT_CC_STATIC_LIBSTDCXX=n
CT_CC_GCC_STATIC_LIBSTDCXX=n
...
```

Now you can build crosstool-ng. It will take 15~20 mins:
```bash
ct-ng build
```

When finished, you should have the toolchain setup in /Volumes/CrossToolNG/x-tools directory.
now go back to the project's directory and set some environment variables:
```bash
export ZEPHYR_GCC_VARIANT=xtools
export XTOOLS_TOOLCHAIN_PATH=/Volumes/CrossToolNG/x-tools
```

To use the same toolchain in future sessions, you can set the variables in the file $HOME/.zephyrrc. For example:
```bash
cat <<EOF > ~/.zephyrrc
export XTOOLS_TOOLCHAIN_PATH=/Volumes/CrossToolNG/x-tools
export ZEPHYR_GCC_VARIANT=xtools
EOF
```

For a new environment, create a symlink for Python2.7:
```bash
ln -s /usr/bin/python2.7 /usr/local/bin/python2
```

Now, you can build and run on QEMU using this command:
```bash
make JS=samples/HelloWorld.js BOARD=qemu_x86 qemu
```

You should see output:
```bash
To exit from QEMU enter: 'CTRL+a, x'
[QEMU] CPU: qemu32
qemu-system-i386: warning: Unknown firmware file in legacy mode: genroms/multiboot.bin

Hello, ZJS world!
```

To build for Arduino 101, you'll need to download the latest Arduino IDE
[here](https://www.arduino.cc/en/guide/macOSX). Once you have the IDE installed,
open the IDE and click on Tools->Board->Board Manager, and install the latest
version of the board support package "Intel Curie Boards". This will install the
 arc-elf compiler located in the following directory:

`$HOME/Library/Arduino15/packages/Intel/tools/arc-elf32/`

Then copy the compiler to where you installed the crosstool-ng toolchain:
```bash
cp –pR $HOME/Library/Arduino15/packages/Intel/tools/arc-elf32 /Volumes/CrossToolNG/
```

The compiler is in a subdirectory, such as 1.6.9+1.0.1. You'll need
to pass this to make to find the right bin file.

You can now build for Arduino 101 (without setting BOARD=, it builds arduino_101 by default)
```bash
make JS=samples/HelloWorld.js ARC_CROSS_COMPILE=/Volumes/CrossToolNG/arc-elf32/1.6.9+1.0.1/bin/arc-elf32-
```

**Note** There's currently a bug that you'll run into a linker issue using the latest Zephyr (1.7.0).
We filed a bug upstream to track this; we'll update this document once that is fixed.

### Other targets like FRDM-K64F or possibly other ARM boards on Mac
These also have limited support currently. The requiremenet is that you'll
need to install the GCC ARM Embedded cross compiler [here](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads)

After you download it, set these environment variables:
```bash
export GCCARMEMB_TOOLCHAIN_PATH="~/Downloads/gcc-arm-none-eabi-6-2017-q1-update/"
export ZEPHYR_GCC_VARIANT=gccarmemb
```

Then you can build like this:
```bash
make JS=samples/HelloWorld.js BOARD=frdm_k64f CROSS_COMPILE=~/Downloads/gcc-arm-none-eabi-6-2017-q1-update/bin/arm-none-eabi-
```

For additional information, see [here](https://www.zephyrproject.org/doc/getting_started/getting_started.html#third-party-x-compilers) on how to setup third-party compilers.

## Supported modules on Linux and Zephyr
There is only partial support for modules on Linux compared to Zephyr. Any hardware
specific module (I2C, UART, GPIO, ADC etc.) is not supported on Linux. Trying
to run a Zephyr specific module on Linux will result in the JavaScript not running
successfully. Below is a complete table of modules and target support. Some
Zephyr targets listed are experimental and have not been fully tested. For this
reason we have the following possibilities for support:

* X - Supported
* E - Experimental, not formally tested by QA but basic functionality verified.
* NT - Not tested at all
* Blank - Not Supported

| Module    | Linux | Arduino 101 | K64F  | nRF52 | Arduino DUE | ST F411RE |
| :---:     | :---: |    :---:    | :---: | :---: |    :---:    |   :---:   |
|HelloWorld |   X   |      X      |   X   |    X  |      X      |     X     |
|  ADC      |       |      X      |       |   NT  |      NT     |     NT    |
|  PWM      |       |      X      |       |   NT  |      NT     |     NT    |
|  GPIO     |       |      X      |   X   |   E   |      NT     |     NT    |
|  I2C      |       |      X      |   X   |   NT  |      NT     |     NT    |
|  BLE      |       |      X      |       |   NT  |             |           |
|  UART     |       |      X      |   X   |   NT  |      NT     |     NT    |
| Sensor    |       |      X      |       |       |             |           |
| Buffer    |   X   |      X      |   X   |   E   |      E      |     E     |
| Console   |   X   |      X      |   X   |   E   |      E      |     E     |
| Event     |   X   |      X      |   X   |   E   |      E      |     E     |
|File System|       |      X      |       |       |             |           |
| OCF       |   X   |      X      |   NT  |   E   |             |           |
|Performance|   X   |      X      |   X   |   E   |      E      |     E     |
| Timers    |   X   |      X      |   X   |   E   |      E      |     E     |
| Dgram     |       |      X      |   NT  |   NT  |             |           |
| Net       |       |      X      |   NT  |   NT  |             |           |
| WebSocket |       |      X      |   NT  |   NT  |             |           |

## Networking with QEMU
QEMU has support for networking features that can be tested on your Linux
desktop. To do this you will need to build a separate "net-tools" project:
```bash
git clone https://gerrit.zephyrproject.org/r/net-tools
cd net-tools
make
```

Open up 2 terminals to run the tools:
Terminal 1:
```bash
sudo ./net-tools/loop-socat.sh
```

If this fails, you may need to install the socat package:
```bash
sudo apt-get install socat
```

Terminal 2:
```bash
./net-tools/loop-slip-tap.sh
```
Then run QEMU as your normally would e.g.
```bash
make BOARD=qemu_x86 JS=samples/OcfServer.js qemu
```

Note: At this point, this setup is relatively unstable. You may experience
crashes or things just not working in general. If the behavior does not seem
normal you can usually fix it by restarting the two scripts and running QEMU
again.

The original instructions document can be found on the Zephyr website
(here)[https://www.zephyrproject.org/doc/subsystems/networking/qemu_setup.html]

## Networking with BLE
It is possible to use IP networking over BLE using 6lowpan. This is explained
in a dedicated [document](./docs/6lowpan-ble.md)
