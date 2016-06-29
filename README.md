The ZephyrJS Project
================

This purpose of this project is to evaluate different JavaScript engines and select one
to port to run on low-memory systems using the Zephyr operating system.

This repository contains scripts and work that does not directly correlate
to one of the cloned upstream trees that we're working with.

File Descriptions
-----------------
```zjs-env.sh``` - Source this file to be able to use tools from ```scripts/``` anywhere.

Subdirectories
--------------
- ```deps/``` - Contains dependency repos and scripts for working with them.
- ```githooks/``` - Currently just one script to use when committing to upstream
    Samsung projects.
- ```samples/``` - Sample JavaScript files that can be build and executed with ```jsrunner```.
- ```scripts/``` - Subdirectory containing tools useful during development.
- ```src/``` - JS API bindings for JerryScript written directly on top of Zephyr.
- ```test/``` - Zephyr applications that test various components.

Setting up your environment
---------------------------
To checkout the other repos ```zjs``` is dependent on:

```
$ cd deps;
$ ./getrepos;
$ ./update
```

Download the latest Zephyr SDK at https://nexus.zephyrproject.org/content/repositories/releases/org/zephyrproject/zephyr-sdk/
```
$ chmod +x zephyr-sdk-0.*.*-i686-setup.run
$ sudo ./zephyr-sdk-0.*.*-i686-setup.run
```

Set up environment variables. From the root directory of the ZephyrJS Project,
run:
```
$ source deps/zephyr/zephyr-env.sh
$ source zjs-env.sh
```
```
$ export ZEPHYR_GCC_VARIANT=zephyr
$ export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk
```
It is recommended that you add the above two lines to your ```.bashrc``` so that you
don't have to type them again.

Getting more space on your Arduino 101
--------------------------------------
By default, Arduino 101 comes with a **144K** X86 partition, but we're able to
pretty safely increase it to **256K**.

See the ```README.bl.md``` for further instructions. Be aware that you should only
use the ```dfu-util``` method of flashing after this.

The instructions below are still refering to the old JTAG method.

Build the *Hello World* sample
----------------------------
(For now, this assumes you have a Flyswatter JTAG device hooked up to your PC
and your Arduino 101.)

```
$ cd samples
$ jsrunner HelloWorld.js
```

```jsrunner``` creates a fresh ```build/``` directory, copies in some config files from
```scripts/template```, generates a ```script.h``` include file from the ```HelloWorld.js```
script, and builds using the ```src/``` files. Then it flashes your Arduino 101.

Build other samples
-------------------
The other samples may require some hardware to be set up and connected; read
the top of each JS file, but then simply pass in the path to the JS file to
```jsrunner``` as with ```HelloWorld.js``` above.

Running JerryScript Unit Tests
------------------------------
```
$ cd apps/jerryscript_test/tests/
$ ./rununittest.sh
```

This will iterate through the unit test one by one. After each test completes, press Ctrl-x
to exit QEMU and continue with the next test.